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
    if (out_len == NULL)
    {
        return HEX_DECODE_INVALID_ARGUMENT;
    }

    *out_len = 0;

    if ((hex_len > 0 && hex == NULL) || (hex_len > 0 && out == NULL))
    {
        return HEX_DECODE_INVALID_ARGUMENT;
    }

    // 两个十六进制字符才能组成一个完整字节。
    if ((hex_len % 2) != 0)
    {
        return HEX_DECODE_INVALID_TEXT;
    }

    const size_t required = hex_len / 2;
    // 先确认容量，避免写到一半才发现输出缓冲区不够。
    if (required > out_capacity)
    {
        return HEX_DECODE_NO_SPACE;
    }

    // 先校验全部字符，避免非法文本留下半份输出。
    for (size_t i = 0; i < required; ++i)
    {
        const int high = hex_nibble((unsigned char)hex[2 * i]);
        const int low = hex_nibble((unsigned char)hex[2 * i + 1]);
        if (high < 0 || low < 0)
        {
            return HEX_DECODE_INVALID_TEXT;
        }
    }

    // 所有字符都通过校验后，再写入完整字节。
    for (size_t i = 0; i < required; ++i)
    {
        const int high = hex_nibble((unsigned char)hex[2 * i]);
        const int low = hex_nibble((unsigned char)hex[2 * i + 1]);
        out[i] = (uint8_t)((high << 4) | low);
    }

    *out_len = required;
    return HEX_DECODE_OK;
}
