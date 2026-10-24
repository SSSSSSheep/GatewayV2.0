#include "gateway/jdy_parser.h"

#include <string.h>

/**
 * @brief 重置解析器
 * @param parser 解析器
 */
static void reset_frame(jdy_parser_t *parser)
{
    parser->state = JDY_STATE_WAIT_F1;
    parser->body_length = 0;
    parser->body_received = 0;
}

jdy_parser_status_t parser_init(jdy_parser_t *parser)
{
    if (parser == NULL)
    {
        return JDY_PARSER_ERR_ARGUMENT;
    }

    memset(parser, 0, sizeof(*parser));
    parser->state = JDY_STATE_WAIT_F1;
    return JDY_PARSER_OK;
}

jdy_parser_status_t parser_feed(jdy_parser_t *parser,
                                const uint8_t *data,
                                size_t len)
{
    size_t i;

    // 校验参数
    if (parser == NULL || (data == NULL && len != 0))
    {
        return JDY_PARSER_ERR_ARGUMENT;
    }

    if (parser->buffered > JDY_PARSER_BUFFER_SIZE)
    {
        return JDY_PARSER_ERR_ARGUMENT;
    }

    if (len > JDY_PARSER_BUFFER_SIZE - parser->buffered)
    {
        return JDY_PARSER_ERR_NO_SPACE;
    }

    for (i = 0; i < len; i++)
    {
        parser->input[parser->write_pos] = data[i];
        parser->write_pos =
            (parser->write_pos + 1U) % JDY_PARSER_BUFFER_SIZE;
        parser->buffered++;
    }

    return JDY_PARSER_OK;
}

jdy_parser_status_t parser_next(jdy_parser_t *parser,
                                jdy_frame_t *out_frame)
{
    // 校验参数
    if (parser == NULL || out_frame == NULL)
    {
        return JDY_PARSER_ERR_ARGUMENT;
    }

    // 缓冲区中还有数据
    while (parser->buffered > 0)
    {
        uint8_t byte = parser->input[parser->read_pos];

        parser->read_pos =
            (parser->read_pos + 1U) % JDY_PARSER_BUFFER_SIZE;
        parser->buffered--;

        switch (parser->state)
        {
        case JDY_STATE_WAIT_F1:
            if (byte == 0xF1)
            {
                parser->state = JDY_STATE_WAIT_DD; // 找到帧头首字节
            }
            break;

        case JDY_STATE_WAIT_DD:
            if (byte == 0xDD)
            {
                parser->state = JDY_STATE_WAIT_LENGTH; // 帧头完整
            }
            else if (byte != 0xF1)
            {
                parser->state = JDY_STATE_WAIT_F1; // 丢弃不匹配噪声
            }
            break;

        case JDY_STATE_WAIT_LENGTH:
            if (byte >= JDY_MIN_BODY_SIZE &&
                byte <= JDY_MAX_BODY_SIZE)
            {
                parser->body_length = byte;
                parser->body_received = 0;
                parser->state = JDY_STATE_RECEIVE_BODY; // 开始收 body
            }
            else
            {
                // 非法长度不作为帧；若当前字节是 F1，保留为候选帧头
                parser->state = (byte == 0xF1)
                                    ? JDY_STATE_WAIT_DD
                                    : JDY_STATE_WAIT_F1;
            }
            break;

        case JDY_STATE_RECEIVE_BODY:
            if (parser->body_received >= parser->body_length)
            {
                reset_frame(parser);
                break;
            }

            parser->body[parser->body_received++] = byte;

            // 如果当前以写入的body总字节数等于body长度，则认为帧接收完毕
            if (parser->body_received == parser->body_length)
            {
                jdy_frame_t frame = {0};
                size_t payload_offset = 2U * JDY_ADDRESS_SIZE;

                memcpy(frame.source,
                       parser->body,
                       JDY_ADDRESS_SIZE);
                memcpy(frame.destination,
                       parser->body + JDY_ADDRESS_SIZE,
                       JDY_ADDRESS_SIZE);

                // 计算消息体长度
                frame.payload_len =
                    parser->body_length - payload_offset;
                memcpy(frame.payload,
                       parser->body + payload_offset,
                       frame.payload_len);

                // 完整帧接收完毕，重置解析器
                reset_frame(parser);
                *out_frame = frame; // 一次调用最多输出一帧
                return JDY_PARSER_OK;
            }
            break;

        default:
            reset_frame(parser);
            break;
        }
    }

    return JDY_PARSER_NEED_MORE;
}