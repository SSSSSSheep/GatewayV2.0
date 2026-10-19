#include "gateway/jdy_parser.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                             \
    do                                               \
    {                                                \
        if (!(condition))                            \
        {                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n",      \
                    __FILE__, __LINE__, #condition); \
            return 1;                                \
        }                                            \
    } while (0)

static int frame_equals(const jdy_frame_t *frame,
                        const uint8_t *source,
                        const uint8_t *destination,
                        const uint8_t *payload,
                        size_t payload_len)
{
    return memcmp(frame->source, source, JDY_ADDRESS_SIZE) == 0 &&
           memcmp(frame->destination,
                  destination,
                  JDY_ADDRESS_SIZE) == 0 &&
           frame->payload_len == payload_len &&
           memcmp(frame->payload, payload, payload_len) == 0;
}

static int test_complete_frame(void)
{
    static const uint8_t input[] = {
        0xF1, 0xDD, 0x07,
        0x23, 0x23,      // source
        0xFF, 0xFF,      // destination
        0x41, 0x42, 0x43 // payload: "ABC"
    };
    static const uint8_t source[] = {0x23, 0x23};
    static const uint8_t destination[] = {0xFF, 0xFF};
    static const uint8_t payload[] = {0x41, 0x42, 0x43};

    jdy_parser_t parser;
    jdy_frame_t frame = {0};

    CHECK(parser_init(&parser) == JDY_PARSER_OK);
    CHECK(parser_feed(&parser, input, sizeof(input)) == JDY_PARSER_OK);
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(frame_equals(&frame, source, destination,
                       payload, sizeof(payload)));
    CHECK(parser.state == JDY_STATE_WAIT_F1);

    return 0;
}

static int test_half_frame(void)
{
    static const uint8_t first_part[] = {
        0xF1, 0xDD, 0x07, 0x23};
    static const uint8_t second_part[] = {
        0x23, 0xFF, 0xFF, 0x41, 0x42, 0x43};
    static const uint8_t source[] = {0x23, 0x23};
    static const uint8_t destination[] = {0xFF, 0xFF};
    static const uint8_t payload[] = {0x41, 0x42, 0x43};

    jdy_parser_t parser;
    jdy_frame_t frame;
    uint8_t before[sizeof(frame)];

    CHECK(parser_init(&parser) == JDY_PARSER_OK);

    memset(&frame, 0xA5, sizeof(frame));
    memcpy(before, &frame, sizeof(frame));

    CHECK(parser_feed(&parser, first_part, sizeof(first_part)) ==
          JDY_PARSER_OK);
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_NEED_MORE);
    CHECK(parser.state == JDY_STATE_RECEIVE_BODY);
    CHECK(parser.body_length == 7);
    CHECK(parser.body_received == 1);
    CHECK(memcmp(&frame, before, sizeof(frame)) == 0);

    CHECK(parser_feed(&parser, second_part, sizeof(second_part)) ==
          JDY_PARSER_OK);
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(frame_equals(&frame, source, destination,
                       payload, sizeof(payload)));

    return 0;
}

static int test_noise_before_frame(void)
{
    static const uint8_t input[] = {
        0xF9, 0xF9, 0x01, 0x01, 0x33, 0x33,
        0xF1, 0xDD, 0x07,
        0x23, 0x23, 0xFF, 0xFF, 0x41, 0x42, 0x43};
    static const uint8_t source[] = {0x23, 0x23};
    static const uint8_t destination[] = {0xFF, 0xFF};
    static const uint8_t payload[] = {0x41, 0x42, 0x43};

    jdy_parser_t parser;
    jdy_frame_t frame = {0};

    CHECK(parser_init(&parser) == JDY_PARSER_OK);
    CHECK(parser_feed(&parser, input, sizeof(input)) == JDY_PARSER_OK);
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(frame_equals(&frame, source, destination,
                       payload, sizeof(payload)));

    return 0;
}

static int test_two_concatenated_frames(void)
{
    static const uint8_t input[] = {
        0xF1, 0xDD, 0x07,
        0x23, 0x23, 0xFF, 0xFF, 0x41, 0x42, 0x43,

        0xF1, 0xDD, 0x05,
        0x00, 0x02, 0x01, 0x01, 0x31};
    static const uint8_t source1[] = {0x23, 0x23};
    static const uint8_t destination1[] = {0xFF, 0xFF};
    static const uint8_t payload1[] = {0x41, 0x42, 0x43};

    static const uint8_t source2[] = {0x00, 0x02};
    static const uint8_t destination2[] = {0x01, 0x01};
    static const uint8_t payload2[] = {0x31};

    jdy_parser_t parser;
    jdy_frame_t frame;
    uint8_t before[sizeof(frame)];

    CHECK(parser_init(&parser) == JDY_PARSER_OK);
    CHECK(parser_feed(&parser, input, sizeof(input)) == JDY_PARSER_OK);

    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(frame_equals(&frame, source1, destination1,
                       payload1, sizeof(payload1)));

    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(frame_equals(&frame, source2, destination2,
                       payload2, sizeof(payload2)));

    memcpy(before, &frame, sizeof(frame));
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_NEED_MORE);
    CHECK(memcmp(&frame, before, sizeof(frame)) == 0);

    return 0;
}

static int test_frame_header_bytes_inside_payload(void)
{
    static const uint8_t input[] = {
        0xF1, 0xDD, 0x06,
        0x00, 0x02, 0x01, 0x01,
        0xF1, 0xDD};
    static const uint8_t source[] = {0x00, 0x02};
    static const uint8_t destination[] = {0x01, 0x01};
    static const uint8_t payload[] = {0xF1, 0xDD};

    jdy_parser_t parser;
    jdy_frame_t frame = {0};

    CHECK(parser_init(&parser) == JDY_PARSER_OK);
    CHECK(parser_feed(&parser, input, sizeof(input)) == JDY_PARSER_OK);
    CHECK(parser_next(&parser, &frame) == JDY_PARSER_OK);
    CHECK(frame_equals(&frame, source, destination,
                       payload, sizeof(payload)));

    return 0;
}

int main(void)
{
    CHECK(test_complete_frame() == 0);
    CHECK(test_half_frame() == 0);
    CHECK(test_noise_before_frame() == 0);
    CHECK(test_two_concatenated_frames() == 0);
    CHECK(test_frame_header_bytes_inside_payload() == 0);

    puts("jdy_parser tests passed");
    return 0;
}