#ifndef GATEWAY_MQTT_H
#define GATEWAY_MQTT_H

#include <stddef.h>

typedef struct
{
    const char *broker_address; // MQTT broker地址，例如 "tcp://localhost:1883"
    const char *client_id;      // MQTT客户端ID
    const char *topic;          // 订阅的主题，例如 "gateway/commands"
    int serial_fd;              // 串口文件描述符
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

#endif // !GATEWAY_MQTT_H
