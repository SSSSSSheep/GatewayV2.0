#include "gateway/message_json.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                             \
    do                                               \
    {                                                \
        if (!(condition))                            \
        {                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n",      \
                    __FILE__, __LINE__, #condition); \
            return 1;                                \
        }                                            \
    } while (0)

static int test_encode_success(void)
{
    gateway_message_t message = {0};
    char json[128] = {0};
    size_t json_len = 0;

    message.conn_type = 1;
    message.id[0] = 0x23;
    message.id[1] = 0x23;
    message.msg[0] = 0x41;
    message.msg[1] = 0x42;
    message.msg[2] = 0x43;
    message.msg_len = 3;

    CHECK(message_json_to_encode(
              &message,
              json,
              sizeof(json),
              &json_len) == MESSAGE_JSON_OK);

    CHECK(strcmp(
              json,
              "{\"conn_type\":1,\"id\":\"2323\",\"msg\":\"414243\"}") == 0);

    CHECK(json_len == strlen(json));
    CHECK(json[json_len] == '\0');

    return 0;
}

static int test_encode_no_space(void)
{
    gateway_message_t message = {0};
    char json[128];
    size_t json_len = 123;

    message.conn_type = 1;
    message.id[0] = 0x00;
    message.id[1] = 0x02;
    message.msg[0] = 0x31;
    message.msg_len = 1;

    memset(json, 0xA5, sizeof(json));

    CHECK(message_json_to_encode(
              &message,
              json,
              1,
              &json_len) == MESSAGE_JSON_ERR_TOO_LARGE);

    CHECK(json_len == 0);
    CHECK((unsigned char)json[0] == 0xA5);

    return 0;
}

static int test_encode_invalid_type(void)
{
    gateway_message_t message = {0};
    char json[128] = {0};
    size_t json_len = 99;

    message.conn_type = 2;
    message.msg[0] = 0x31;
    message.msg_len = 1;

    CHECK(message_json_to_encode(
              &message,
              json,
              sizeof(json),
              &json_len) == MESSAGE_JSON_ERR_TYPE);

    CHECK(json_len == 99);

    return 0;
}

static int test_encode_invalid_length(void)
{
    gateway_message_t message = {0};
    char json[128] = {0};
    size_t json_len = 99;

    message.conn_type = 1;
    message.msg_len = 0;

    CHECK(message_json_to_encode(
              &message,
              json,
              sizeof(json),
              &json_len) == MESSAGE_JSON_ERR_RANGE);

    CHECK(json_len == 99);

    return 0;
}

static int test_encode_null_argument(void)
{
    gateway_message_t message = {0};
    char json[128] = {0};
    size_t json_len = 0;

    CHECK(message_json_to_encode(
              NULL,
              json,
              sizeof(json),
              &json_len) == MESSAGE_JSON_ERR_ARGUMENT);

    CHECK(message_json_to_encode(
              &message,
              NULL,
              sizeof(json),
              &json_len) == MESSAGE_JSON_ERR_ARGUMENT);

    CHECK(message_json_to_encode(
              &message,
              json,
              sizeof(json),
              NULL) == MESSAGE_JSON_ERR_ARGUMENT);

    return 0;
}

int main(void)
{
    CHECK(test_encode_success() == 0);
    CHECK(test_encode_no_space() == 0);
    CHECK(test_encode_invalid_type() == 0);
    CHECK(test_encode_invalid_length() == 0);
    CHECK(test_encode_null_argument() == 0);

    puts("message_json_to_encode tests passed");
    return 0;
}