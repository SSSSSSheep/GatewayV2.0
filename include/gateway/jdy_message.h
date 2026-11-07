#ifndef GATEWAY_JDY_MESSAGE_H
#define GATEWAY_JDY_MESSAGE_H

#include "gateway/jdy_parser.h"
#include "gateway/message_json.h"

#define JDY_GATEWAY_ADDRESS_HIGH 0x01U
#define JDY_GATEWAY_ADDRESS_LOW 0x01U

/* 只接受发给本机的单播，并校验内部地址、长度；不提供身份认证或CRC校验。 */
int jdy_frame_to_message(const jdy_frame_t *frame,
                         const uint8_t local_address[JDY_ADDRESS_SIZE],
                         gateway_message_t *message);

#endif
