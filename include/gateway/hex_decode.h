#ifndef GATEWAY_HEX_DECODE_H
#define GATEWAY_HEX_DECODE_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    HEX_DECODE_OK = 0,                // 解码成功
    HEX_DECODE_INVALID_ARGUMENT = -1, // 非法参数
    HEX_DECODE_INVALID_TEXT = -2,     // 非法文本
    HEX_DECODE_NO_SPACE = -3          // 空间不足
} hex_decode_status_t;

/**
 * @brief 16进制字符串解码
 * @param hex 十六进制消息体文本；长度为0时可为空
 * @param hex_len 文本长度，必须为偶数
 * @param out 解码后的数据
 * @param out_capacity out的容量
 * @param out_len 解码后的数据长度
 * @return 成功时 out_len 为解码后的字节数；非法文本或空间不足时 out 保持
 *         不变且 out_len 为0。hex_len 为0时 hex 和 out 可为空。
 */
hex_decode_status_t hex_decode(const char *hex,
                               size_t hex_len,
                               uint8_t *out,
                               size_t out_capacity,
                               size_t *out_len);

#endif
