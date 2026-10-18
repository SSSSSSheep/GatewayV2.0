#ifndef GATEWAY_HEX_DECODE_H
#define GATEWAY_HEX_DECODE_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    HEX_DECODE_OK = 0,
    HEX_DECODE_INVALID_ARGUMENT = -1,
    HEX_DECODE_INVALID_TEXT = -2,
    HEX_DECODE_NO_SPACE = -3
} hex_decode_status_t;

hex_decode_status_t hex_decode(const char *hex,
                               size_t hex_len,
                               uint8_t *out,
                               size_t out_capacity,
                               size_t *out_len);

#endif