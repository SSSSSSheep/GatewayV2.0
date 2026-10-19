#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "gateway/message_json.h"
#include "gateway/serial.h"
#include "gateway/jdy_at.h"

int main(void)
{
    /*
    JSON -> 内部消息:
    conn_type:1
    id:"0002"->00 02
    msg:"31"->31
    msg_len:1
    */

    const char json[] = "{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"31\"}";

    gateway_message_t message = {0};
    uint8_t packet[64] = {0};
    size_t packet_len = 0;

    printf("==== 1. 输入JSON ====\n");
    printf("%s\n", json);

    int result = message_json_to_decode(json, sizeof(json) - 1, &message);

    // 失败时打印并停止，避免继续使用无效结果
    if (result != 0)
    {
        fprintf(stderr, "JSON解析失败: %d\n", result);
        return 1;
    }

    printf("==== 2. JSON -> 内部消息 ====\n");
    printf("conn_type: %d\n", message.conn_type);
    printf("id        = %02X %02X\n",
           (unsigned int)message.id[0],
           (unsigned int)message.id[1]);
    printf("msg_len = %zu\n", (size_t)message.msg_len);

    printf("msg = ");
    for (size_t i = 0; i < message.msg_len; i++)
    {
        printf("%02X ", (unsigned int)message.msg[i]);
    }
    printf("\n");

    /*
     * 当前示例输出，共 16 字节：
     *
     * [0~6]   [7]  [8~9] [10~11] [12] [13] [14~15]
     * AT+MESH  00   00 02  01 01   01   31   0D 0A
     * 头部     CMD  目标   源地址  长度 数据 CRLF
     */
    result = jdy_at_build(
        &message,
        packet,
        sizeof(packet),
        &packet_len);

    if (result != 0)
    {
        fprintf(stderr, "JDY 组包失败，错误码：%d\n", result);
        return 1;
    }

    printf("=== 3. 蓝牙指令 ===\n");
    printf("packet_len = %zu\n", packet_len);
    printf("HEX        =");

    for (size_t i = 0; i < packet_len; ++i)
    {
        printf(" %02X", (unsigned int)packet[i]);
    }
    printf("\n\n");

    printf("=== 4. 当前示例的字节位置图 ===\n");
    printf("[0~6]   [7]  [8~9] [10~11] [12] [13] [14~15]\n");
    printf("AT+MESH  00   00 02  01 01   01   31   0D 0A\n");
    printf("Header  CMD  DST    SRC     LEN  DATA CRLF\n");

        /*
     * packet 中是原始字节，不是 HEX 字符串：
     *
     * [0~6]   [7]  [8~9] [10~11] [12] [13] [14~15]
     * AT+MESH  00   00 02  01 01   01   31   0D 0A
     *
     * write() 直接发送 packet_len 个字节，包括其中的 00。
     */
    const char *serial_path = "/tmp/rk_gateway";
    // RK3568 的 UART3 请改为 "/dev/ttyS3"。

    printf("=== 4. 打开真实串口 ===\n");
    printf("设备：%s，115200 8N1\n", serial_path);

    int fd = serial_open(serial_path);
    if (fd < 0)
    {
        return 1;
    }

    printf("\n=== 5. 发送蓝牙指令 ===\n");

    ssize_t written = serial_write(fd, packet, packet_len);

    if (written < 0)
    {
        perror("serial_write");
        serial_close(fd);
        return 1;
    }

    printf("write 返回：%zd，期望：%zu\n", written, packet_len);

    // 本次先报告短写，不增加发送队列和重试机制。
    if ((size_t)written != packet_len)
    {
        fprintf(stderr, "本次未写完全部数据，停止演示\n");
        serial_close(fd);
        return 1;
    }

    printf("\n=== 6. 阻塞等待串口返回 ===\n");
    printf("没有数据时会停在这里，可按 Ctrl+C 结束。\n");
    fflush(stdout);

    uint8_t rx_buffer[128];
    ssize_t received = serial_read(fd, rx_buffer, sizeof(rx_buffer));

    if (received < 0)
    {
        perror("serial_read");
        serial_close(fd);
        return 1;
    }

    printf("read 返回：%zd 字节\n", received);
    printf("RX HEX =");

    for (size_t i = 0; i < (size_t)received; ++i)
    {
        printf(" %02X", (unsigned int)rx_buffer[i]);
    }
    printf("\n");

    if (serial_close(fd) < 0)
    {
        perror("serial_close");
        return 1;
    }

    printf("串口已关闭，演示结束。\n");
    return 0;
}