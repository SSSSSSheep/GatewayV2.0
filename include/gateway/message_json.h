#ifndef GATEWAY_MESSAGE_JSON_H
#define GATEWAY_MESSAGE_JSON_H

#include <stddef.h>
#include <stdint.h>

#define GATEWAY_ID_SIZE 2
#define GATEWAY_MAX_MSG_SIZE 12
#define GATEWAY_MAX_JSON_SIZE 512

typedef struct
{
    uint8_t conn_type;
    uint8_t id[GATEWAY_ID_SIZE];
    uint8_t msg[GATEWAY_MAX_MSG_SIZE];
    size_t msg_len;
} gateway_message_t;

typedef enum
{
    MESSAGE_JSON_OK = 0,
    MESSAGE_JSON_ERR_ARGUMENT = -1,
    MESSAGE_JSON_ERR_TOO_LARGE = -2,
    MESSAGE_JSON_ERR_JSON = -3,
    MESSAGE_JSON_ERR_FIELD = -4,
    MESSAGE_JSON_ERR_TYPE = -5,
    MESSAGE_JSON_ERR_RANGE = -6,
    MESSAGE_JSON_ERR_HEX = -7
} message_json_status_t;

/*
 * payload 不要求以 NUL 结尾。
 * payload_len 是实际数据长度，不包含额外的 NUL 终止符。
 *
 * 当前协议：
 * - 只接受 conn_type、id、msg，且各出现一次。
 * - conn_type 必须为数值 1。
 * - id 必须解码为两字节地址。
 * - msg 必须解码为 1~12 字节数据。
 *
 * 成功时填充 *out；失败时 *out 保持不变。
 */
message_json_status_t message_json_to_decode(const void *payload,
                                             size_t payload_len,
                                             gateway_message_t *out_msg);

#endif // GATEWAY_MESSAGE_JSON_H