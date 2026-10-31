#ifndef GATEWAY_MESSAGE_JSON_H
#define GATEWAY_MESSAGE_JSON_H

#include <stddef.h>
#include <stdint.h>

// 设备地址长度，单位为字节。
#define GATEWAY_ID_SIZE 2
// 消息体最大长度，单位为字节。
#define GATEWAY_MAX_MSG_SIZE 12
// JSON 消息体最大长度，单位为字节。
#define GATEWAY_MAX_JSON_SIZE 512

// JSON 与串口协议之间传递的网关消息。
typedef struct
{
    uint8_t conn_type;                 // 连接类型，目前只支持 1
    uint8_t id[GATEWAY_ID_SIZE];       // id，用于标识设备
    uint8_t msg[GATEWAY_MAX_MSG_SIZE]; // 原始消息体字节
    size_t msg_len;                    // 消息体长度
} gateway_message_t;

// JSON 解码状态
typedef enum
{
    MESSAGE_JSON_OK = 0,             // 成功
    MESSAGE_JSON_ERR_ARGUMENT = -1,  // 参数错误
    MESSAGE_JSON_ERR_TOO_LARGE = -2, // 数据过长
    MESSAGE_JSON_ERR_JSON = -3,      // JSON 解析错误
    MESSAGE_JSON_ERR_FIELD = -4,     // JSON 字段错误
    MESSAGE_JSON_ERR_TYPE = -5,      // JSON 类型错误
    MESSAGE_JSON_ERR_RANGE = -6,     // JSON 范围错误
    MESSAGE_JSON_ERR_HEX = -7        // 十六进制解码错误
} message_json_status_t;

/**
 * @brief 将 JSON 解码为 gateway_message_t。
 * @param payload JSON 消息体，不要求以 NUL 结尾。
 * @param payload_len 实际消息体长度，不包含额外的 NUL 终止符。
 * @param out_msg 解码结果。
 * @return 只接受 conn_type、id、msg 且各出现一次；conn_type 必须为1，
 *         id 解码后为两字节，msg 解码后为1～12字节。成功时填充 out_msg，
 *         失败时 out_msg 不变；返回值为 message_json_status_t。
 */
message_json_status_t message_json_to_decode(const void *payload,
                                             size_t payload_len,
                                             gateway_message_t *out_msg);

/**
 * @brief 将 gateway_message_t 编码为 JSON。
 * @param message 消息。
 * @param out_json 编码结果。
 * @param out_capacity 编码结果容量。
 * @param out_len 编码结果长度。
 * @return 成功时写入 JSON 和长度；失败时 out_json 不变。参数、连接类型或
 *         消息体长度错误时 out_len 可能保持原值；进入编码阶段后会先清零，
 *         成功时再写入实际长度。
 */
message_json_status_t message_json_to_encode(const gateway_message_t *message,
                                             char *out_json,
                                             size_t out_capacity,
                                             size_t *out_len);

#endif // GATEWAY_MESSAGE_JSON_H
