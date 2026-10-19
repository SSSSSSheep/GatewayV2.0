#include "gateway/hex_decode.h"

/**
 * @brief 将单个16进制字符转换为对应的数值
 * @param c 16进制字符
 * @return 对应的数值，如果字符不是16进制字符，则返回-1
 */
static int hex_nibble(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return (int)(c - 'a') + 10;
    if (c >= 'A' && c <= 'F')
        return (int)(c - 'A') + 10;
    return -1;
}

hex_decode_status_t hex_decode(const char *hex,
                               size_t hex_len,
                               uint8_t *out,
                               size_t out_capacity,
                               size_t *out_len)
{
    // 检查参数是否合法
    if (out_len == NULL)
    {
        return HEX_DECODE_INVALID_ARGUMENT;
    }

    *out_len = 0;

    // 如果hex为空，则直接返回
    if ((hex_len > 0 && hex == NULL) || (hex_len > 0 && out == NULL))
    {
        return HEX_DECODE_INVALID_ARGUMENT;
    }

    // 检查hex的长度是否为偶数
    if ((hex_len % 2) != 0)
    {
        return HEX_DECODE_INVALID_TEXT;
    }

    // 检查out是否有足够的空间存储解码后的数据
    const size_t required = hex_len / 2;
    // 如果required > out_capacity，则返回错误
    if (required > out_capacity)
    {
        return HEX_DECODE_NO_SPACE;
    }

    // 转换单个字符的高低位
    for (size_t i = 0; i < required; ++i)
    {
        const int high = hex_nibble((unsigned char)hex[2 * i]);
        const int low = hex_nibble((unsigned char)hex[2 * i + 1]);
        if (high < 0 || low < 0)
        {
            return HEX_DECODE_INVALID_TEXT;
        }
    }

    // 将高低位组合成字节
    for (size_t i = 0; i < required; ++i)
    {
        const int high = hex_nibble((unsigned char)hex[2 * i]);
        const int low = hex_nibble((unsigned char)hex[2 * i + 1]);
        out[i] = (uint8_t)((high << 4) | low);
    }

    // 返回成功
    *out_len = required;
    return HEX_DECODE_OK;
}