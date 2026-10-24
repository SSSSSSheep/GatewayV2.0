#ifndef GATEWAY_JDY_AT_H
#define GATEWAY_JDY_AT_H

#include <stddef.h>
#include <stdint.h>

#include "gateway/message_json.h"

/**蓝牙数据固定头
 * 固定开销：AT+MESH(7) + CMD(1) + 目标地址(2)
 *          + 源地址(2) + 数据长度(1) + CRLF(2) = 15 字节。
 * 实际整包长度 = 15 + msg_len；例如业务数据 1 字节时，整包为 16 字节。
 */
#define JDY_AT_FIXED_SIZE 15
// AT+MESH 命令最大长度
#define JDY_AT_MAX_SIZE \
    (JDY_AT_FIXED_SIZE + GATEWAY_MAX_MSG_SIZE)

// 蓝牙状态机
typedef enum
{
    JDY_AT_OK = 0,            // 组包成功 输出数组和输出长度有效
    JDY_AT_ERR_ARGUMENT = -1, // 参数错误
    JDY_AT_ERR_TYPE = -2,     // 连接类型错误
    JDY_AT_ERR_LENGTH = -3,   // 消息长度不符合组包要求
    JDY_AT_ERR_NO_SPACE = -4  // 输出数组容量不足 放不下完整指令
} jdy_at_status_t;

/*
 * 将消息编码为 AT+MESH 命令。
 *
 * msg：已经解码的内部消息。
 * out：调用者提供的输出缓冲区。
 * out_capacity：out 的实际容量。
 * out_len：成功时返回命令长度；失败时为0。
 *
 * 失败时不修改 out。
 * msg、out、out_len 所指存储区域不得重叠。
 *
 * 本函数只负责安全编码，不验证无线链路支持的最大长度。
 */

/**
 * @brief 将消息编码为 AT+MESH 命令。
 * @param msg 已经解码的内部消息。
 * @param out 调用者提供的输出缓冲区。
 * @param out_capacity out 的实际容量。
 * @param out_len 成功时返回命令长度；失败时为0。
 * @return jdy_at_status_t
 */
jdy_at_status_t jdy_at_build(const gateway_message_t *msg,
                             uint8_t *out,
                             size_t out_capacity,
                             size_t *out_len);

#endif // !GATEWAY_JDY_AT_H
