#include "../../include/gateway/hex_decode.h"

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

int main(void)
{
    uint8_t out[8];
    size_t out_len;
    hex_decode_status_t status;

    /* 1. 正常地址 */
    memset(out, 0xA5, sizeof(out));
    out_len = 99;

    status = hex_decode("0002", 4, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_OK);
    CHECK(out_len == 2);
    CHECK(out[0] == 0x00);
    CHECK(out[1] == 0x02);
    CHECK(out[2] == 0xA5);

    /* 2. 正常控制字节 */
    status = hex_decode("31", 2, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_OK);
    CHECK(out_len == 1);
    CHECK(out[0] == 0x31);

    /* 3. 大小写混合 */
    status = hex_decode("aF", 2, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_OK);
    CHECK(out_len == 1);
    CHECK(out[0] == 0xAF);

    /* 4. 空输入 */
    status = hex_decode("", 0, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_OK);
    CHECK(out_len == 0);

    /* 5. 奇数长度 */
    out_len = 99;
    status = hex_decode("123", 3, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_INVALID_TEXT);
    CHECK(out_len == 0);

    /* 6. 非法字符 */
    out_len = 99;
    status = hex_decode("3G", 2, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_INVALID_TEXT);
    CHECK(out_len == 0);

    /* 7. 输出空间不足 */
    memset(out, 0xA5, sizeof(out));
    status = hex_decode("0002", 4, out, 1, &out_len);

    CHECK(status == HEX_DECODE_NO_SPACE);
    CHECK(out_len == 0);
    CHECK(out[0] == 0xA5);

    /* 8. 输入指针为空，长度非零 */
    status = hex_decode(NULL, 2, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_INVALID_ARGUMENT);
    CHECK(out_len == 0);

    /* 9. 输出指针为空 */
    status = hex_decode("31", 2, NULL, 1, &out_len);

    CHECK(status == HEX_DECODE_INVALID_ARGUMENT);
    CHECK(out_len == 0);

    /* 10. 输出长度指针为空 */
    status = hex_decode("31", 2, out, sizeof(out), NULL);

    CHECK(status == HEX_DECODE_INVALID_ARGUMENT);

    /* 11. 中途遇到非法字符，不应留下部分输出 */
    memset(out, 0xA5, sizeof(out));
    status = hex_decode("313G", 4, out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_INVALID_TEXT);
    CHECK(out_len == 0);

    for (size_t i = 0; i < sizeof(out); ++i) {
        CHECK(out[i] == 0xA5);
    }

    /* 12. 不带 NUL 结尾的输入 */
    const char raw_hex[] = {'0', '0', '0', '2'};

    status = hex_decode(raw_hex, sizeof(raw_hex),
                        out, sizeof(out), &out_len);

    CHECK(status == HEX_DECODE_OK);
    CHECK(out_len == 2);
    CHECK(out[0] == 0x00);
    CHECK(out[1] == 0x02);

    /* 13. 输出容量刚好足够 */
    uint8_t exact[2];

    status = hex_decode("00FF", 4,
                        exact, sizeof(exact), &out_len);

    CHECK(status == HEX_DECODE_OK);
    CHECK(out_len == 2);
    CHECK(exact[0] == 0x00);
    CHECK(exact[1] == 0xFF);

    puts("hex_decode tests passed");
    return 0;
}