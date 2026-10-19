#include "gateway/jdy_at.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                             \
    do                                               \
    {                                                \
        if (!(condition))                            \
        {                                            \
            fprintf(stderr, "FAIL: %s:%d: %s\n",     \
                    __FILE__, __LINE__, #condition); \
            return 1;                                \
        }                                            \
    } while (0)

static int all_equal(const uint8_t *data,
                     size_t len,
                     uint8_t value)
{
    for (size_t i = 0; i < len; ++i)
    {
        if (data[i] != value)
        {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    gateway_message_t msg = {
        .conn_type = 1,
        .id = {0x00, 0x02},
        .msg = {0x31},
        .msg_len = 1};

    /* 独立写出的预期向量，不调用被测函数生成。 */
    const uint8_t expected[] = {
        0x41, 0x54, 0x2B, 0x4D, 0x45, 0x53, 0x48,
        0x00,
        0x00, 0x02,
        0x01, 0x01,
        0x01,
        0x31,
        0x0D, 0x0A};

    uint8_t out[JDY_AT_MAX_SIZE + 1];
    size_t out_len = 99;

    /* 1. 开灯命令：逐字节比较全部16字节。 */
    memset(out, 0xA5, sizeof(out));

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), &out_len) == JDY_AT_OK);

    CHECK(out_len == sizeof(expected));
    CHECK(memcmp(out, expected, sizeof(expected)) == 0);

    /* 有效输出之后不能被修改。 */
    CHECK(all_equal(out + out_len,
                    sizeof(out) - out_len, 0xA5));

    /* 2. 关灯命令。 */
    msg.msg[0] = 0x30;

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), &out_len) == JDY_AT_OK);

    CHECK(out_len == 16);
    CHECK(out[13] == 0x30);
    CHECK(out[14] == '\r');
    CHECK(out[15] == '\n');

    /* 3. 业务数据包含00，不能发生字符串截断。 */
    msg.msg[0] = 0x00;

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), &out_len) == JDY_AT_OK);

    CHECK(out_len == 16);
    CHECK(out[13] == 0x00);
    CHECK(out[14] == '\r');
    CHECK(out[15] == '\n');

    /* 4. 容量恰好为16字节。 */
    msg.msg[0] = 0x31;

    CHECK(jdy_at_build(
              &msg, out, 16, &out_len) == JDY_AT_OK);

    CHECK(out_len == 16);

    /* 5. 容量少一字节：失败且输出完全不变。 */
    memset(out, 0xA5, sizeof(out));
    out_len = 99;

    CHECK(jdy_at_build(
              &msg, out, 15, &out_len) == JDY_AT_ERR_NO_SPACE);

    CHECK(out_len == 0);
    CHECK(all_equal(out, sizeof(out), 0xA5));

    /* 6. 最大应用长度：验证内存边界，不代表无线硬件支持。 */
    msg.msg_len = GATEWAY_MAX_MSG_SIZE;

    for (size_t i = 0; i < msg.msg_len; ++i)
    {
        msg.msg[i] = (uint8_t)i;
    }

    memset(out, 0xA5, sizeof(out));

    CHECK(jdy_at_build(
              &msg, out, JDY_AT_MAX_SIZE, &out_len) == JDY_AT_OK);

    CHECK(out_len == JDY_AT_MAX_SIZE);
    CHECK(out[12] == GATEWAY_MAX_MSG_SIZE);

    for (size_t i = 0; i < msg.msg_len; ++i)
    {
        CHECK(out[13 + i] == (uint8_t)i);
    }

    CHECK(out[out_len - 2] == '\r');
    CHECK(out[out_len - 1] == '\n');
    CHECK(out[out_len] == 0xA5);

    /* 7. 错误的连接类型。 */
    memset(out, 0xA5, sizeof(out));
    msg.conn_type = 2;

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), &out_len) == JDY_AT_ERR_TYPE);

    CHECK(out_len == 0);
    CHECK(all_equal(out, sizeof(out), 0xA5));

    msg.conn_type = 1;

    /* 8. 零长度业务消息。 */
    msg.msg_len = 0;

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), &out_len) == JDY_AT_ERR_LENGTH);

    CHECK(out_len == 0);
    CHECK(all_equal(out, sizeof(out), 0xA5));

    /* 9. 长度超过结构体中的数组容量。 */
    msg.msg_len = GATEWAY_MAX_MSG_SIZE + 1;

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), &out_len) == JDY_AT_ERR_LENGTH);

    CHECK(out_len == 0);
    CHECK(all_equal(out, sizeof(out), 0xA5));

    /* 10. 极大长度也应先拒绝，不能发生长度计算溢出。 */
    msg.msg_len = SIZE_MAX;

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), &out_len) == JDY_AT_ERR_LENGTH);

    CHECK(out_len == 0);

    msg.msg_len = 1;

    /* 11. 空指针参数。 */
    CHECK(jdy_at_build(
              NULL, out, sizeof(out), &out_len) == JDY_AT_ERR_ARGUMENT);

    CHECK(out_len == 0);

    CHECK(jdy_at_build(
              &msg, NULL, sizeof(out), &out_len) == JDY_AT_ERR_ARGUMENT);

    CHECK(out_len == 0);

    CHECK(jdy_at_build(
              &msg, out, sizeof(out), NULL) == JDY_AT_ERR_ARGUMENT);

    puts("jdy_at tests passed");
    return 0;
}