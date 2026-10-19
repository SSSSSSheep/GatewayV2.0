#ifndef GATEWAY_MESSAGE_JSON_H
#define GATEWAY_MESSAGE_JSON_H

#include <stddef.h>
#include <stdint.h>

// ID 的最大长度
#define GATEWAY_ID_SIZE 2
// 消息的最大长度
#define GATEWAY_MAX_MSG_SIZE 12
// 消息的最大 JSON 长度
#define GATEWAY_MAX_JSON_SIZE 512

// 网关消息结构体，用于存储解码后的 JSON 数据
typedef struct
{
    uint8_t conn_type;                 // 连接类型，目前只支持 1
    uint8_t id[GATEWAY_ID_SIZE];       // id，用于标识设备
    uint8_t msg[GATEWAY_MAX_MSG_SIZE]; // 消息内容
    size_t msg_len;                    // 消息长度
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
 *
 * @brief 将 JSON 解码为 gateway_message_t。
 */

/**
 * @brief 将 JSON 解码为 gateway_message_t。
 * @param payload JSON 数据。
 * @param payload_len JSON 数据长度。
 * @param out_msg 解码结果。
 * @return 解码状态。
 */
message_json_status_t message_json_to_decode(const void *payload,
                                             size_t payload_len,
                                             gateway_message_t *out_msg);

#endif // GATEWAY_MESSAGE_JSON_H