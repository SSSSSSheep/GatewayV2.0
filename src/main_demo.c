#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "gateway/message_json.h"
#include "gateway/serial.h"
#include "gateway/jdy_at.h"
#include "gateway/mqtt.h"
#include "gateway/io_loop.h"

int main(void)
{
    /*
     * 这条JSON只用于启动前展示解码和组包结果。
     * 真正的下行消息由MQTT主题 gateway/down 接收。
     */
    const char json[] =
        "{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"31\"}";

    gateway_message_t message = {0};
    uint8_t packet[64] = {0};
    size_t packet_len = 0;

    printf("=== 1. 输入JSON ===\n");
    printf("%s\n", json);

    int result = message_json_to_decode(
        json,
        sizeof(json) - 1,
        &message);

    if (result != 0)
    {
        fprintf(stderr, "JSON解析失败：%d\n", result);
        return 1;
    }

    printf("=== 2. JSON -> 内部消息 ===\n");
    printf("conn_type = %u\n", (unsigned int)message.conn_type);

    printf("id        = %02X %02X\n",
           (unsigned int)message.id[0],
           (unsigned int)message.id[1]);

    printf("msg_len   = %zu\n",
           message.msg_len);

    printf("msg       =");

    for (size_t i = 0; i < message.msg_len; i++)
    {
        printf(" %02X", (unsigned int)message.msg[i]);
    }

    printf("\n");

    result = jdy_at_build(
        &message,
        packet,
        sizeof(packet),
        &packet_len);

    if (result != JDY_AT_OK)
    {
        fprintf(stderr, "JDY组包失败：%d\n", result);
        return 1;
    }

    printf("=== 3. 蓝牙指令示例 ===\n");
    printf("packet_len = %zu\n", packet_len);
    printf("HEX        =");

    for (size_t i = 0; i < packet_len; i++)
    {
        printf(" %02X", (unsigned int)packet[i]);
    }

    printf("\n");
    printf("[0~6]   [7]  [8~9] [10~11] [12] [13] [14~15]\n");
    printf("AT+MESH  00   00 02  01 01   01   31   0D 0A\n");
    printf("头部     CMD  目标   源地址  长度 消息体 CRLF\n");

    /*
     * PC测试使用socat创建的虚拟串口。
     * 上板时改为RK3568实际设备，例如 /dev/ttyS3。
     */
    const char *serial_path = "/tmp/rk_gateway";

    printf("=== 4. 打开串口 ===\n");
    printf("设备：%s，115200 8N1\n", serial_path);

    int fd = serial_open(serial_path);

    if (fd < 0)
    {
        return 1;
    }

    /*
     * 当前阶段不在这里发送固定packet。
     * 所有真正的下行数据都由MQTT回调接收后组包并发送。
     */
    gateway_mqtt_config_t mqtt_config = {
        .broker_address = "tcp://192.168.0.101:1883",
        .client_id = "rk_gateway_v2_pc",
        .topic = "gateway/down",
        .up_topic = "gateway/up",
        .serial_fd = fd};

    if (gateway_mqtt_start(&mqtt_config) != 0)
    {
        fprintf(stderr, "MQTT启动失败\n");
        serial_close(fd);
        return 1;
    }

    printf("=== 5. 等待MQTT下行消息 ===\n");
    printf("主题：gateway/down\n");
    printf("按 Ctrl+C 结束程序。\n");
    fflush(stdout);

    printf("进入串口事件循环，等待上行数据...\n");

    if (io_loop_run(fd) != 0)
    {
        gateway_mqtt_stop();
        serial_close(fd);
        return 1;
    }

    gateway_mqtt_stop();
    serial_close(fd);
    return 0;
}