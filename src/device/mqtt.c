#include "gateway/mqtt.h"
#include "gateway/jdy_at.h"
#include "gateway/message_json.h"
#include "gateway/serial.h"

#include <MQTTClient.h>
#include <stdio.h>
#include <string.h>

#define GATEWAY_MQTT_QOS 0             // QOS等级，0表示最多一次，1表示至少一次，2表示只有一次
#define GATEWAY_MQTT_TIMEOUT_MS 10000L // 连接超时时间

static MQTTClient mqtt_client;  // MQTT客户端句柄
static int mqtt_serial_fd = -1; // 串口文件描述符
static int mqtt_started = 0;    // MQTT是否已经启动

static const char *mqtt_up_topic = NULL; // MQTT上行主题

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
 * @brief 处理MQTT下行消息 解码JSON 组装JDY指令并写入串口
 * @param context 上下文
 * @param topic_name 主题名称
 * @param topic_len 主题长度
 * @param message MQTT消息
 * @return 1 表示消息已处理并释放；0 表示暂未处理、要求 Paho 保留消息以便重试。
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

    // 解码JSON
    gateway_message_t decoded = {0};

    message_json_status_t json_status = message_json_to_decode(message->payload, (size_t)message->payloadlen, &decoded);

    if (json_status != MESSAGE_JSON_OK)
    {
        fprintf(stderr, "MQTT JSON解码失败：%d\n", json_status);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topic_name);
        return -1;
    }

    // 组装JDY指令
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

    // 写入串口文件
    ssize_t written = serial_write(mqtt_serial_fd, packet, packet_len);
    if (written < 0)
    {
        perror("MQTT下行串口写入");
    }
    else
    {
        printf("串口写入：%zd/%zu 字节\n", written, packet_len);
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

    // 初始化MQTT客户端
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

    // 设置MQTT客户端回调函数
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

    // 设置MQTT客户端连接选项
    MQTTClient_connectOptions options = MQTTClient_connectOptions_initializer;

    options.keepAliveInterval = 20; // 设置MQTT的心跳保活时间
    options.cleansession = 1;       // 设置MQTT的清理会话选项

    // 设置MQTT客户端连接选项
    status = MQTTClient_connect(mqtt_client, &options);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTTClient_connect failed: %d\n", status);
        MQTTClient_destroy(&mqtt_client);
        return -1;
    }

    // 订阅MQTT主题
    status = MQTTClient_subscribe(mqtt_client, config->topic, GATEWAY_MQTT_QOS);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTTClient_subscribe failed: %d\n", status);
        MQTTClient_disconnect(mqtt_client, GATEWAY_MQTT_TIMEOUT_MS);
        MQTTClient_destroy(&mqtt_client);
        return -1;
    }

    mqtt_started = 1;
    return 0;
}

void gateway_mqtt_stop(void)
{
    if (!mqtt_started)
    {
        return;
    }

    MQTTClient_disconnect(mqtt_client, GATEWAY_MQTT_TIMEOUT_MS);
    MQTTClient_destroy(&mqtt_client);

    mqtt_serial_fd = -1;
    mqtt_started = 0;
}

int gateway_mqtt_publish_up(const gateway_message_t *message)
{
    // json编码
    char json[GATEWAY_MAX_JSON_SIZE] = {0};
    size_t json_len = 0;
    MQTTClient_message mqtt_message = MQTTClient_message_initializer;
    int status = 0;

    // 判断是否已经启动
    if (!mqtt_started || message == NULL || mqtt_up_topic == NULL)
    {
        return -1;
    }

    // 将下位机返回的指令解码 -> json
    if (message_json_to_encode(message, json, sizeof(json), &json_len) != MESSAGE_JSON_OK)
    {
        fprintf(stderr, "上行JSON编码失败\n");
        return -1;
    }

    // 设置MQTT消息
    mqtt_message.payload = json;
    mqtt_message.payloadlen = (int)json_len;
    mqtt_message.qos = GATEWAY_MQTT_QOS;
    mqtt_message.retained = 0; // 设置消息是否保留

    // 发布MQTT消息
    status = MQTTClient_publishMessage(mqtt_client, mqtt_up_topic, &mqtt_message, NULL);

    if (status != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTT 上行发布失败：%d\n", status);
        return -1;
    }

    printf("MQTT 上行发布：%.*s\n", (int)json_len, json);
    return 0;
}
