#include "gateway/jdy_message.h"

#include <string.h>

int jdy_frame_to_message(const jdy_frame_t *frame,
                         const uint8_t local_address[JDY_ADDRESS_SIZE],
                         gateway_message_t *message)
{
    const size_t header_size = JDY_ADDRESS_SIZE + 1U;
    gateway_message_t decoded = {0};

    if (frame == NULL || local_address == NULL || message == NULL ||
        frame->payload_len <= header_size ||
        frame->payload_len > sizeof(frame->payload))
    {
        return -1;
    }

    if (memcmp(frame->destination, local_address, JDY_ADDRESS_SIZE) != 0 ||
        memcmp(frame->payload, frame->source, JDY_ADDRESS_SIZE) != 0 ||
        frame->payload_len - header_size > sizeof(decoded.msg) ||
        (size_t)frame->payload[JDY_ADDRESS_SIZE] != frame->payload_len - header_size)
    {
        return -1;
    }

    decoded.conn_type = 1;
    memcpy(decoded.id, frame->source, sizeof(decoded.id));
    decoded.msg_len = frame->payload_len - header_size;
    memcpy(decoded.msg, frame->payload + header_size, decoded.msg_len);
    *message = decoded;
    return 0;
}
