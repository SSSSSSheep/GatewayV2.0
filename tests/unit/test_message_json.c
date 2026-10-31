#include "../../include/gateway/message_json.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                      \
    do {                                                      \
        if (!(condition)) {                                   \
            fprintf(stderr, "FAIL: %s:%d: %s\n",               \
                    __FILE__, __LINE__, #condition);          \
            return 1;                                         \
        }                                                     \
    } while (0)

/* 检查错误码，并确认失败时没有修改输出。 */
static int expect_error(const void *payload,
                        size_t len,
                        message_json_status_t expected)
{
    gateway_message_t out;
    unsigned char before[sizeof(out)];

    memset(&out, 0xA5, sizeof(out));
    memcpy(before, &out, sizeof(out));

    CHECK(message_json_to_decode(payload, len, &out) == expected);
    CHECK(memcmp(before, &out, sizeof(out)) == 0);

    return 0;
}

int main(void)
{
    const char valid[] =
        "{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"31\"}";

    gateway_message_t out = {0};

    /* 正常消息 */
    CHECK(message_json_to_decode(
        valid, strlen(valid), &out) == MESSAGE_JSON_OK);

    CHECK(out.conn_type == 1);
    CHECK(out.id[0] == 0x00);
    CHECK(out.id[1] == 0x02);
    CHECK(out.msg_len == 1);
    CHECK(out.msg[0] == 0x31);

    /* 不带 NUL 结尾，模拟 MQTT 消息体 */
    char raw[sizeof(valid) - 1];
    memcpy(raw, valid, sizeof(raw));

    CHECK(message_json_to_decode(
        raw, sizeof(raw), &out) == MESSAGE_JSON_OK);

    /* 字段顺序变化、大小写 HEX、二进制零 */
    const char binary[] =
        "{\"msg\":\"00aF\",\"id\":\"AbCd\",\"conn_type\":1}";

    CHECK(message_json_to_decode(
        binary, strlen(binary), &out) == MESSAGE_JSON_OK);

    CHECK(out.id[0] == 0xAB && out.id[1] == 0xCD);
    CHECK(out.msg_len == 2);
    CHECK(out.msg[0] == 0x00 && out.msg[1] == 0xAF);

    /* 最大合法消息体：12 字节 */
    const char maximum[] =
        "{\"conn_type\":1,\"id\":\"0002\","
        "\"msg\":\"000102030405060708090A0B\"}";

    CHECK(message_json_to_decode(
        maximum, strlen(maximum), &out) == MESSAGE_JSON_OK);

    CHECK(out.msg_len == GATEWAY_MAX_MSG_SIZE);
    CHECK(out.msg[11] == 0x0B);

    /* JSON 结尾允许空白 */
    const char whitespace[] =
        "{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"31\"}\r\n";

    CHECK(message_json_to_decode(
        whitespace, strlen(whitespace), &out) == MESSAGE_JSON_OK);

    struct {
        const char *json;
        message_json_status_t expected;
    } cases[] = {
        /* 非法 JSON、错误根类型 */
        {"{", MESSAGE_JSON_ERR_JSON},
        {"[]", MESSAGE_JSON_ERR_JSON},

        /* 缺字段 */
        {"{}", MESSAGE_JSON_ERR_FIELD},
        {"{\"id\":\"0002\",\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_FIELD},
        {"{\"conn_type\":1,\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_FIELD},
        {"{\"conn_type\":1,\"id\":\"0002\"}",
         MESSAGE_JSON_ERR_FIELD},

        /* 类型错误 */
        {"{\"conn_type\":\"1\",\"id\":\"0002\",\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_TYPE},
        {"{\"conn_type\":true,\"id\":\"0002\",\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_TYPE},
        {"{\"conn_type\":1,\"id\":2,\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_TYPE},
        {"{\"conn_type\":1,\"id\":\"0002\",\"msg\":null}",
         MESSAGE_JSON_ERR_TYPE},

        /* 范围错误 */
        {"{\"conn_type\":1.5,\"id\":\"0002\",\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_RANGE},
        {"{\"conn_type\":2,\"id\":\"0002\",\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_RANGE},
        {"{\"conn_type\":1,\"id\":\"02\",\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_RANGE},
        {"{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"\"}",
         MESSAGE_JSON_ERR_RANGE},

        /* 非法 HEX、奇数长度 */
        {"{\"conn_type\":1,\"id\":\"00GG\",\"msg\":\"31\"}",
         MESSAGE_JSON_ERR_HEX},
        {"{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"3G\"}",
         MESSAGE_JSON_ERR_HEX},
        {"{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"123\"}",
         MESSAGE_JSON_ERR_HEX},

        /* 超过 12 字节 */
        {"{\"conn_type\":1,\"id\":\"0002\","
         "\"msg\":\"000102030405060708090A0B0C\"}",
         MESSAGE_JSON_ERR_RANGE},

        /* 重复字段 */
        {"{\"conn_type\":1,\"id\":\"0002\","
         "\"msg\":\"31\",\"msg\":\"30\"}",
         MESSAGE_JSON_ERR_FIELD},

        /* 未知字段 */
        {"{\"conn_type\":1,\"id\":\"0002\","
         "\"msg\":\"31\",\"extra\":1}",
         MESSAGE_JSON_ERR_FIELD},

        /* 结尾垃圾内容 */
        {"{\"conn_type\":1,\"id\":\"0002\",\"msg\":\"31\"}junk",
         MESSAGE_JSON_ERR_JSON},

        /* 转义 NUL */
        {"{\"conn_type\":1,\"id\":\"0002\","
         "\"msg\":\"31\\u0000GG\"}",
         MESSAGE_JSON_ERR_JSON}
    };

    for (size_t i = 0;
         i < sizeof(cases) / sizeof(cases[0]);
         ++i) {
        CHECK(expect_error(
            cases[i].json,
            strlen(cases[i].json),
            cases[i].expected) == 0);
    }

    /* 参数错误 */
    CHECK(expect_error(
        NULL, 1, MESSAGE_JSON_ERR_ARGUMENT) == 0);

    CHECK(expect_error(
        valid, 0, MESSAGE_JSON_ERR_ARGUMENT) == 0);

    CHECK(message_json_to_decode(
        valid, strlen(valid), NULL) == MESSAGE_JSON_ERR_ARGUMENT);

    /* 输入超过 JSON 总长度上限 */
    char too_large[GATEWAY_MAX_JSON_SIZE + 1];
    memset(too_large, ' ', sizeof(too_large));

    CHECK(expect_error(
        too_large, sizeof(too_large),
        MESSAGE_JSON_ERR_TOO_LARGE) == 0);

    /* 实际嵌入的 NUL */
    const char embedded_nul[] = {'{', '\0', '}'};

    CHECK(expect_error(
        embedded_nul, sizeof(embedded_nul),
        MESSAGE_JSON_ERR_JSON) == 0);

    /*
     * sizeof(valid) 包含末尾 NUL；
     * 本接口约定输入长度不包含它。
     */
    CHECK(expect_error(
        valid, sizeof(valid), MESSAGE_JSON_ERR_JSON) == 0);

    /* 重复成功和失败路径，配合 LeakSanitizer 检查释放。 */
    for (int i = 0; i < 1000; ++i) {
        CHECK(message_json_to_decode(
            valid, strlen(valid), &out) == MESSAGE_JSON_OK);

        const char missing[] = "{\"conn_type\":1}";

        CHECK(expect_error(
            missing, strlen(missing),
            MESSAGE_JSON_ERR_FIELD) == 0);
    }

    puts("message_json tests passed");
    return 0;
}
