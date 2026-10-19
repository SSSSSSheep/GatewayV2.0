#ifndef GATEWAY_JDY_PARSER_H
#define GATEWAY_JDY_PARSER_H

#include <stddef.h>
#include <stdint.h>

/*
 * 暂按当前协议文档采用L=5～16：
 * 两组地址占4字节，业务负载为1～12字节。
 * 模块实际长度范围仍需手册或抓包确认。
 */
#define JDY_MIN_BODY_SIZE 5  // 最小负载长度
#define JDY_MAX_BODY_SIZE 16 // 最大负载长度
#define JDY_ADDRESS_SIZE 2   // 地址长度

// 最大负载长度 = 最大总长度 - 2 * 地址长度
#define JDY_MAX_PAYLOAD_SIZE \
    (JDY_MAX_BODY_SIZE - 2 * JDY_ADDRESS_SIZE)

// 解析器输入缓存大小，必须大于最大总长度
#define JDY_PARSER_BUFFER_SIZE 256

// 帧头长度
typedef enum
{
    JDY_STATE_WAIT_F1 = 0, // 等待帧头 F1
    JDY_STATE_WAIT_DD,     // 等待帧头 DD
    JDY_STATE_WAIT_LENGTH, // 等待长度字节
    JDY_STATE_RECEIVE_BODY // 接收负载
} jdy_parser_state_t;

// 解析器状态
typedef enum
{
    JDY_PARSER_OK = 0,            // 成功输出一帧
    JDY_PARSER_NEED_MORE = 1,     // 需要更多数据，未输出帧
    JDY_PARSER_ERR_ARGUMENT = -1, // 参数错误
    JDY_PARSER_ERR_NO_SPACE = -2  // 输入缓存空间不足
} jdy_parser_status_t;

/*
 * 输入帧：
 *   F1 DD 07 | 23 23 | FF FF | 41 42 43
 *
 * 输出：
 *   source      = 23 23
 *   destination = FF FF
 *   payload_len = 3
 *   payload     = 41 42 43
 *
 * payload保持原始字节，不解析自定义业务头。
 */
typedef struct
{
    uint8_t source[JDY_ADDRESS_SIZE];      // 源地址
    uint8_t destination[JDY_ADDRESS_SIZE]; // 目标地址
    uint8_t payload[JDY_MAX_PAYLOAD_SIZE]; // 业务负载
    size_t payload_len;                    // 业务负载长度
} jdy_frame_t;

/* 一个设备对应一个上下文，不能被多个线程无锁并发操作。 */
typedef struct
{
    uint8_t input[JDY_PARSER_BUFFER_SIZE]; // 输入缓存
    size_t read_pos;                       // 读指针，指向下一个待处理字节
    size_t write_pos;                      // 写指针，指向下一个可写入字节
    size_t buffered;                       // 缓存中有效字节数 = write_pos - read_pos

    jdy_parser_state_t state;        // 当前状态机状态
    uint8_t body[JDY_MAX_BODY_SIZE]; // 当前帧负载缓存
    size_t body_length;              // 当前帧负载长度，0表示未接收长度字节
    size_t body_received;            // 当前帧负载已接收字节数
} jdy_parser_t;

/*
 * 初始化或重置上下文，丢弃所有缓存及半包。
 * parser为空时返回参数错误。
 */

/**
 * @brief 初始化或重置上下文，丢弃所有缓存及半包。
 * @param parser 解析器上下文
 * @return jdy_parser_status_t
 */
jdy_parser_status_t parser_init(jdy_parser_t *parser);

/*
 * 将字节追加到输入缓存，不推进状态机。
 *
 * 成功：本次数据全部接收。
 * 空间不足：返回NO_SPACE，本次数据一个字节也不接收。
 * len为0时允许data为NULL；parser必须有效。
 */
/**
 * @brief 将字节追加到输入缓存，不推进状态机。
 * @param parser 解析器上下文
 * @param data 输入数据
 * @param len 输入数据长度
 * @return jdy_parser_status_t
 */
jdy_parser_status_t parser_feed(jdy_parser_t *parser,
                                const uint8_t *data,
                                size_t len);

/*
 * 消费缓存并推进状态机，每次最多输出一帧。
 *
 * OK：输出完整帧，状态复位；剩余输入保留。
 * NEED_MORE：没有完整帧，保留半包状态，out_frame不变。
 * 参数错误：返回ERR_ARGUMENT。
 *
 * 噪声及非法长度会被跳过，再继续寻找帧头。
 * parser与out_frame的存储区域不得重叠。
 */
/**
 * @brief 消费缓存并推进状态机，每次最多输出一帧。
 * @param parser 解析器上下文
 * @param out_frame 输出帧
 * @return jdy_parser_status_t
 */
jdy_parser_status_t parser_next(jdy_parser_t *parser,
                                jdy_frame_t *out_frame);

#endif