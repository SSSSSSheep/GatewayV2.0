#ifndef GATEWAY_JDY_AT_H
#define GATEWAY_JDY_AT_H

#include <stddef.h>
#include <stdint.h>

#include "gateway/message_json.h"

#define JDY_AT_FIXED_SIZE 15
#define JDY_AT_MAX_SIZE \
    (JDY_AT_FIXED_SIZE + GATEWAY_MAX_MSG_SIZE)

typedef enum
{
    JDY_AT_OK = 0,
    JDY_AT_ERR_ARGUMENT = -1,
    JDY_AT_ERR_TYPE = -2,
    JDY_AT_ERR_LENGTH = -3,
    JDY_AT_ERR_NO_SPACE = -4
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
