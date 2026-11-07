#include "gateway/jdy_message.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

static int rejected_unchanged(const jdy_frame_t *frame, const uint8_t address[2])
{
    gateway_message_t message;
    unsigned char before[sizeof(message)];
    memset(&message, 0xA5, sizeof(message));
    memcpy(before, &message, sizeof(message));
    CHECK(jdy_frame_to_message(frame, address, &message) == -1);
    CHECK(memcmp(before, &message, sizeof(message)) == 0);
    return 0;
}

int main(void)
{
    static const uint8_t gateway_address[2] = {0x01, 0x01};
    static const uint8_t node_address[2] = {0x00, 0x02};
    // 独立写出温度24的实际上行布局，以及发给节点的开灯布局。
    static const uint8_t temperature[] = {
        0xF1, 0xDD, 0x09, 0x00, 0x02, 0x01, 0x01,
        0x00, 0x02, 0x02, 0x32, 0x34};
    static const uint8_t light_on[] = {
        0xF1, 0xDD, 0x08, 0x01, 0x01, 0x00, 0x02,
        0x01, 0x01, 0x01, 0x31};
    jdy_parser_t parser;
    jdy_frame_t frame;
    gateway_message_t message;

    CHECK(parser_init(&parser) == JDY_PARSER_OK);
    CHECK(parser_feed(&parser, temperature, sizeof(temperature)) == JDY_PARSER_OK);
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(jdy_frame_to_message(&frame, gateway_address, &message) == 0);
    CHECK(message.id[0] == 0x00 && message.id[1] == 0x02);
    CHECK(message.msg_len == 2 && message.msg[0] == 0x32 && message.msg[1] == 0x34);
    CHECK(rejected_unchanged(&frame, node_address) == 0);

    frame.destination[0] = 0xFF;
    frame.destination[1] = 0xFF;
    CHECK(rejected_unchanged(&frame, gateway_address) == 0);
    frame.destination[0] = 0x01;
    frame.destination[1] = 0x01;
    frame.payload[0] ^= 1U;
    CHECK(rejected_unchanged(&frame, gateway_address) == 0);
    frame.payload[0] ^= 1U;
    frame.payload[2] = 3;
    CHECK(rejected_unchanged(&frame, gateway_address) == 0);
    frame.payload_len = 3;
    CHECK(rejected_unchanged(&frame, gateway_address) == 0);
    frame.payload_len = sizeof(frame.payload) + 1U;
    CHECK(rejected_unchanged(&frame, gateway_address) == 0);

    CHECK(parser_feed(&parser, light_on, sizeof(light_on)) == JDY_PARSER_OK);
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(jdy_frame_to_message(&frame, node_address, &message) == 0);
    CHECK(message.id[0] == 0x01 && message.id[1] == 0x01);
    CHECK(message.msg_len == 1 && message.msg[0] == 0x31);

    frame.payload_len = JDY_MAX_PAYLOAD_SIZE;
    frame.payload[2] = 9;
    memset(frame.payload + 3, 0x55, 9);
    CHECK(jdy_frame_to_message(&frame, node_address, &message) == 0);
    CHECK(message.msg_len == 9 && message.msg[8] == 0x55);
    frame.payload[2] = 10;
    CHECK(rejected_unchanged(&frame, node_address) == 0);
    CHECK(rejected_unchanged(NULL, node_address) == 0);
    CHECK(rejected_unchanged(&frame, NULL) == 0);
    CHECK(jdy_frame_to_message(&frame, node_address, NULL) == -1);

    puts("jdy_message tests passed");
    return 0;
}
