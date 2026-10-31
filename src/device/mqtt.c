#include "gateway/mqtt.h"
#include "gateway/jdy_at.h"
#include "gateway/message_json.h"
#include "gateway/serial.h"

#include <MQTTClient.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define GATEWAY_MQTT_QOS 0                 // QoS：0 至多一次，1 至少一次，2 恰好一次。
#define GATEWAY_MQTT_TIMEOUT_MS 10000L     // MQTTClient_disconnect() 的等待时间，单位为毫秒。
#define GATEWAY_MQTT_UP_QUEUE_CAPACITY 64U // MQTT上行消息队列容量

/* 有界队列把串口解析和 MQTT 网络发布隔开，避免网络等待拖住 epoll。 */
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    gateway_message_t items[GATEWAY_MQTT_UP_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    int stopping;
} mqtt_up_queue_t;

static mqtt_up_queue_t mqtt_up_queue; // MQTT上行消息队列
static pthread_t mqtt_up_thread;      // MQTT上行消息处理线程
static int mqtt_up_thread_started;    // MQTT上行消息处理线程是否已经启动

static MQTTClient mqtt_client;  // MQTT客户端句柄
static int mqtt_serial_fd = -1; // 串口文件描述符
static int mqtt_started = 0;    // MQTT是否已经启动

static const char *mqtt_up_topic = NULL;         // MQTT上行主题
static const char *mqtt_down_topic = NULL;
static gateway_serial_tx_queue_t *mqtt_tx_queue; // 串口发送队列，承接 MQTT 下行指令。

/**
 * @brief 同步发布 MQTT 上行消息
 * @param message 消息
 * @return 0 表示成功，-1 表示失败
 */
static int gateway_mqtt_publish_up(const gateway_message_t *message);

/**
 * @brief MQTT上行消息处理线程
 * @param arg 线程参数
 * @return void
 */
static void *mqtt_up_publish_worker(void *arg);

static int mqtt_up_publisher_stop(void);

/**
 * @brief MQTT连接回调函数
 * @param context 上下文
 * @param cause 连接失败原因
 * @return void
 */
static void mqtt_connection_lost(void *context, char *cause)
{
    (void)context;

    fprintf(stderr, "MQTT connection lost: %s\n", cause != NULL ? cause : "未知原因");
}

/**
 * @brief 解码 MQTT 下行消息并将 JDY 指令投递到串口发送队列
 * @param context 上下文
 * @param topic_name 主题名称
 * @param topic_len 主题长度
 * @param message MQTT消息
 * @return 1 表示消息已消费并由本回调释放；0 表示暂未消费、由 Paho 保留重试；错误分支还会返回 -1，其语义需按当前 Paho 版本确认。
 */
static int mqtt_message_arrived(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    (void)context;
    (void)topic_len;

    if (message == NULL)
    {
        fprintf(stderr, "MQTT message is NULL\n");
        return -1;
    }

    printf("MQTT收到主题：%s\n",
           topic_name != NULL ? topic_name : "(未知)");
    printf("MQTT收到JSON：%.*s\n",
           message->payloadlen,
           (const char *)message->payload);

    gateway_message_t decoded = {0};

    message_json_status_t json_status = message_json_to_decode(message->payload, (size_t)message->payloadlen, &decoded);

    if (json_status != MESSAGE_JSON_OK)
    {
        fprintf(stderr, "MQTT JSON解码失败：%d\n", json_status);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topic_name);
        return -1;
    }

    uint8_t packet[JDY_AT_MAX_SIZE] = {0};
    size_t packet_len = 0;

    jdy_at_status_t at_status = jdy_at_build(&decoded, packet, sizeof(packet), &packet_len);

    if (at_status != JDY_AT_OK)
    {
        fprintf(stderr, "JDY组包失败，错误码：%d\n", at_status);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topic_name);
        return 1;
    }

    printf("准备发送串口，长度：%zu\n,HEX:", packet_len);

    for (size_t i = 0; i < packet_len; i++)
    {
        printf("%02X ", (unsigned int)packet[i]);
    }

    printf("\n");

    if (mqtt_serial_fd < 0)
    {
        fprintf(stderr, "串口未打开\n");
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topic_name);
        return 1;
    }

    // MQTT 回调只投递数据，由 IO 循环统一写串口，避免回调直接操作串口。
    if (gateway_serial_tx_queue_push(
            mqtt_tx_queue,
            packet,
            packet_len) != 0)
    {
        fprintf(stderr, "串口发送队列写入失败：%s\n", strerror(errno));
    }
    else
    {
        printf("串口帧已加入发送队列\n");
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);

    return 1;
}

/**
 * @brief MQTT消息已发送回调函数
 * @param context 上下文
 * @param token 消息标识符
 */
static void mqtt_message_delivered(void *context, MQTTClient_deliveryToken token)
{
    (void)context;
    (void)token;
}

static int mqtt_up_publisher_start(void)
{
    int result = pthread_mutex_init(&mqtt_up_queue.mutex, NULL);
    if (result != 0)
    {
        errno = result;
        return -1;
    }

    result = pthread_cond_init(&mqtt_up_queue.cond, NULL);
    if (result != 0)
    {
        pthread_mutex_destroy(&mqtt_up_queue.mutex);
        errno = result;
        return -1;
    }

    /* 队列同步对象就绪后再启动消费者，避免线程访问未初始化状态。 */
    result = pthread_create(&mqtt_up_thread, NULL, mqtt_up_publish_worker, &mqtt_up_queue);
    if (result != 0)
    {
        pthread_cond_destroy(&mqtt_up_queue.cond);
        pthread_mutex_destroy(&mqtt_up_queue.mutex);
        errno = result;
        return -1;
    }

    mqtt_up_thread_started = 1;
    return 0;
}

int gateway_mqtt_start(const gateway_mqtt_config_t *config)
{
    if (config == NULL ||
        config->broker_address == NULL ||
        config->client_id == NULL ||
        config->topic == NULL ||
        config->up_topic == NULL ||
        config->serial_fd < 0)
    {
        return -1;
    }

    mqtt_serial_fd = config->serial_fd;
    mqtt_up_topic = config->up_topic;
    mqtt_down_topic = config->topic;
    mqtt_tx_queue = config->tx_queue;

    int status = MQTTClient_create(&mqtt_client,
                                   config->broker_address,
                                   config->client_id,
                                   MQTTCLIENT_PERSISTENCE_NONE,
                                   NULL);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTTClient_create failed: %d\n", status);
        return -1;
    }

    status = MQTTClient_setCallbacks(mqtt_client,
                                     NULL,
                                     mqtt_connection_lost,
                                     mqtt_message_arrived,
                                     mqtt_message_delivered);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTTClient_setCallbacks failed: %d\n", status);
        MQTTClient_destroy(&mqtt_client);
        return -1;
    }

    MQTTClient_connectOptions options = MQTTClient_connectOptions_initializer;

    options.keepAliveInterval = 20; // 设置MQTT的心跳保活时间
    options.cleansession = 1;       // 设置MQTT的清理会话选项

    status = MQTTClient_connect(mqtt_client, &options);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTTClient_connect failed: %d\n", status);
        MQTTClient_destroy(&mqtt_client);
        return -1;
    }

    status = MQTTClient_subscribe(mqtt_client, config->topic, GATEWAY_MQTT_QOS);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTTClient_subscribe failed: %d\n", status);
        MQTTClient_disconnect(mqtt_client, GATEWAY_MQTT_TIMEOUT_MS);
        MQTTClient_destroy(&mqtt_client);
        return -1;
    }

    if (mqtt_up_publisher_start() != 0)
    {
        fprintf(stderr, "MQTT上行发布线程启动失败：%s\n", strerror(errno));
        MQTTClient_disconnect(mqtt_client, GATEWAY_MQTT_TIMEOUT_MS);
        MQTTClient_destroy(&mqtt_client);
        mqtt_serial_fd = -1;
        mqtt_up_topic = NULL;
        mqtt_tx_queue = NULL;
        return -1;
    }

    mqtt_started = 1;
    return 0;
}

int gateway_mqtt_enqueue_up(const gateway_message_t *message)
{
    mqtt_up_queue_t *queue = &mqtt_up_queue;

    if (message == NULL || !mqtt_started || !mqtt_up_thread_started)
    {
        errno = EINVAL;
        return -1;
    }

    /* 入队发生在 epoll 线程，锁忙时立即拒收，不能在这里等待发布线程。 */
    int result = pthread_mutex_trylock(&queue->mutex);
    if (result != 0)
    {
        errno = (result == EBUSY) ? EAGAIN : result;
        return -1;
    }

    /* 队列已停止或已满时拒收新消息，不覆盖已排队的温度数据。 */
    if (queue->stopping || queue->count >= GATEWAY_MQTT_UP_QUEUE_CAPACITY)
    {
        pthread_mutex_unlock(&queue->mutex);
        errno = queue->stopping ? ECANCELED : ENOSPC;
        return -1;
    }

    /* 复制消息到队列，避免调用者的栈上数据在异步发布时失效。 */
    queue->items[queue->tail] = *message;
    queue->tail = (queue->tail + 1U) % GATEWAY_MQTT_UP_QUEUE_CAPACITY;

    queue->count++;

    /* 新消息入队后唤醒正在等待的发布线程。 */
    result = pthread_cond_signal(&queue->cond);
    if (result != 0)
    {
        queue->tail = (queue->tail + GATEWAY_MQTT_UP_QUEUE_CAPACITY - 1U) %
                      GATEWAY_MQTT_UP_QUEUE_CAPACITY;
        queue->count--;
        pthread_mutex_unlock(&queue->mutex);
        errno = result;
        return -1;
    }

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

void gateway_mqtt_stop(void)
{
    if (!mqtt_started)
    {
        return;
    }

    if (mqtt_up_publisher_stop() != 0)
    {
        fprintf(stderr, "MQTT上行发布线程停止失败：%s\n", strerror(errno));
        return;
    }

    MQTTClient_disconnect(mqtt_client, GATEWAY_MQTT_TIMEOUT_MS);
    MQTTClient_destroy(&mqtt_client);

    mqtt_serial_fd = -1;
    mqtt_started = 0;
}

static int gateway_mqtt_publish_up(const gateway_message_t *message)
{
    char json[GATEWAY_MAX_JSON_SIZE] = {0};
    size_t json_len = 0;
    MQTTClient_message mqtt_message = MQTTClient_message_initializer;
    int status = 0;

    if (!mqtt_started || message == NULL || mqtt_up_topic == NULL)
    {
        return -1;
    }

    if (message_json_to_encode(message, json, sizeof(json), &json_len) != MESSAGE_JSON_OK)
    {
        fprintf(stderr, "上行JSON编码失败\n");
        return -1;
    }

    mqtt_message.payload = json;
    mqtt_message.payloadlen = (int)json_len;
    mqtt_message.qos = GATEWAY_MQTT_QOS;
    mqtt_message.retained = 0; // 状态上报不保留，避免新订阅者收到过期温度。

    /* Paho 的同步发布只放在工作线程里，不能阻塞串口事件循环。 */
    status = MQTTClient_publishMessage(mqtt_client, mqtt_up_topic, &mqtt_message, NULL);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTT 上行发布失败：%d\n", status);
        return status;
    }

    printf("MQTT 上行发布：%.*s\n", (int)json_len, json);
    return 0;
}

static int mqtt_up_is_stopping(mqtt_up_queue_t *queue)
{
    int result = pthread_mutex_lock(&queue->mutex);
    if (result != 0)
    {
        return 1;
    }

    int stopping = queue->stopping;
    pthread_mutex_unlock(&queue->mutex);
    return stopping;
}

static int mqtt_up_reconnect(void)
{
    MQTTClient_connectOptions options = MQTTClient_connectOptions_initializer;
    options.keepAliveInterval = 20;
    options.cleansession = 1;
    options.connectTimeout = 3;

    int status = MQTTClient_connect(mqtt_client, &options);
    if (status != MQTTCLIENT_SUCCESS)
    {
        return status;
    }

    /* clean session 不保留旧订阅，连接恢复后要重新订阅下行主题。 */
    status = MQTTClient_subscribe(mqtt_client, mqtt_down_topic, GATEWAY_MQTT_QOS);
    if (status != MQTTCLIENT_SUCCESS)
    {
        MQTTClient_disconnect(mqtt_client, GATEWAY_MQTT_TIMEOUT_MS);
        return status;
    }

    fprintf(stderr, "MQTT连接已恢复，下行主题已重新订阅\n");
    return MQTTCLIENT_SUCCESS;
}

static void *mqtt_up_publish_worker(void *arg)
{
    mqtt_up_queue_t *queue = arg;

    while (1)
    {
        int result = pthread_mutex_lock(&queue->mutex);
        if (result != 0)
        {
            fprintf(stderr, "MQTT上行队列加锁失败：%s\n", strerror(result));
            return NULL;
        }

        /* 条件变量可能提前唤醒，每次醒来都要重新检查队列状态。 */
        while (queue->count == 0 && !queue->stopping)
        {
            result = pthread_cond_wait(&queue->cond, &queue->mutex);
            if (result != 0)
            {
                pthread_mutex_unlock(&queue->mutex);
                fprintf(stderr, "MQTT上行队列等待失败：%s\n", strerror(result));
                return NULL;
            }
        }

        if (queue->count == 0 && queue->stopping)
        {
            pthread_mutex_unlock(&queue->mutex);
            return NULL;
        }

        /* 先取走消息并释放队列锁，再调用 Paho，避免网络等待挡住新消息入队。 */
        gateway_message_t message = queue->items[queue->head];
        queue->head = (queue->head + 1U) % GATEWAY_MQTT_UP_QUEUE_CAPACITY;
        queue->count--;

        /* 消息已取出并更新队列状态；先解锁再调用 Paho，避免网络等待挡住入队。 */
        pthread_mutex_unlock(&queue->mutex);

        /* 当前消息留在工作线程局部变量中，重连成功后再尝试发布。 */
        int publish_status = gateway_mqtt_publish_up(&message);
        while (publish_status == MQTTCLIENT_DISCONNECTED)
        {
            int reconnect_status;
            do
            {
                if (mqtt_up_is_stopping(queue))
                {
                    fprintf(stderr, "MQTT停止时取消上行重试，未发送消息不会保留到下次启动\n");
                    return NULL;
                }

                reconnect_status = mqtt_up_reconnect();
                if (reconnect_status != MQTTCLIENT_SUCCESS)
                {
                    fprintf(stderr, "MQTT重连失败：%d\n", reconnect_status);
                    if (mqtt_up_is_stopping(queue))
                    {
                        fprintf(stderr, "MQTT停止时取消上行重试，未发送消息不会保留到下次启动\n");
                        return NULL;
                    }
                    sleep(1);
                }
            } while (reconnect_status != MQTTCLIENT_SUCCESS);

            publish_status = gateway_mqtt_publish_up(&message);
        }

        if (publish_status != MQTTCLIENT_SUCCESS)
        {
            fprintf(stderr, "MQTT上行消息发布失败：%d\n", publish_status);
        }
    }
}

static int mqtt_up_publisher_stop(void)
{
    if (!mqtt_up_thread_started)
    {
        return 0;
    }

    int result = pthread_mutex_lock(&mqtt_up_queue.mutex);
    if (result != 0)
    {
        errno = result;
        return -1;
    }

    /* 唤醒休眠中的发布线程，让它排空队列后退出。 */
    mqtt_up_queue.stopping = 1;
    result = pthread_cond_broadcast(&mqtt_up_queue.cond);
    pthread_mutex_unlock(&mqtt_up_queue.mutex);

    if (result != 0)
    {
        errno = result;
        return -1;
    }

    /* 等发布线程退出后再断开客户端，避免线程仍在使用 MQTTClient。 */
    result = pthread_join(mqtt_up_thread, NULL);

    if (result != 0)
    {
        errno = result;
        return -1;
    }

    mqtt_up_thread_started = 0;
    pthread_cond_destroy(&mqtt_up_queue.cond);
    pthread_mutex_destroy(&mqtt_up_queue.mutex);
    return 0;
}