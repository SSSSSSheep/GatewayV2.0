#ifndef GATEWAY_MQTT_H
#define GATEWAY_MQTT_H

#include <stddef.h>
#include "gateway/message_json.h"
#include "gateway/serial_tx_queue.h"

typedef struct
{
    const char *broker_address;          // MQTT broker地址，例如 "tcp://localhost:1883"
    const char *client_id;               // MQTT客户端ID
    const char *topic;                   // 订阅的主题，例如 "gateway/commands"
    const char *up_topic;                // 发布上行消息的主题
    gateway_serial_tx_queue_t *tx_queue; // 串口发送队列，供 MQTT 下行回调投递指令帧
    int serial_fd;                       // 串口文件描述符
} gateway_mqtt_config_t;

/**
 * @brief 启动MQTT客户端
 * @param config MQTT客户端配置
 * @return 0表示成功，-1表示失败
 */
int gateway_mqtt_start(const gateway_mqtt_config_t *config);

/**
 * @brief 停止MQTT客户端
 */
void gateway_mqtt_stop(void);

/**
 * @brief 发布上行消息
 * @param message 上行消息
 * @return 0表示成功，-1表示失败
 */
int gateway_mqtt_publish_up(const gateway_message_t *message);

#endif // !GATEWAY_MQTT_H
