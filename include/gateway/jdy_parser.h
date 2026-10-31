#ifndef GATEWAY_JDY_PARSER_H
#define GATEWAY_JDY_PARSER_H

#include <stddef.h>
#include <stdint.h>

/*
 * 暂按当前协议文档采用 L=5～16：
 * 帧体包含源地址、目标地址和消息体。
 * 两组地址占4字节，消息体为1～12字节。
 * 模块实际长度范围仍需手册或抓包确认。
 */
#define JDY_MIN_BODY_SIZE 5  // 帧体最小长度
#define JDY_MAX_BODY_SIZE 16 // 帧体最大长度
#define JDY_ADDRESS_SIZE 2   // 单个地址长度

// 消息体上限由帧体上限扣除两组地址长度得到。
#define JDY_MAX_PAYLOAD_SIZE \
    (JDY_MAX_BODY_SIZE - 2 * JDY_ADDRESS_SIZE)

// 输入环形缓存容量，可缓存多帧数据
#define JDY_PARSER_BUFFER_SIZE 256

// 解析器状态
typedef enum
{
    JDY_STATE_WAIT_F1 = 0, // 等待帧头 F1
    JDY_STATE_WAIT_DD,     // 收到F1 等待帧头 DD
    JDY_STATE_WAIT_LENGTH, // 收到F1 DD 等待body长度
    JDY_STATE_RECEIVE_BODY // 接收源地址、目标地址和消息体
} jdy_parser_state_t;

// 解析器状态码
typedef enum
{
    JDY_PARSER_OK = 0,            // 成功输出一帧
    JDY_PARSER_NEED_MORE = 1,     // 缓存中没有完整帧，需要更多数据
    JDY_PARSER_ERR_ARGUMENT = -1, // 参数错误
    JDY_PARSER_ERR_NO_SPACE = -2  // 输入环形缓存空间不足
} jdy_parser_status_t;

/*
 * 输入帧：
 *   F1 DD 07 | 23 23 | FF FF | 41 42 43
 *   帧头      | 源地址 | 目标地址 | 消息体
 * 输出：
 *   source      = 23 23
 *   destination = FF FF
 *   消息体长度 = 3
 *   消息体     = 41 42 43
 *
 * 消息体保留原始字节，通用帧解析器不解释其内部格式。
 */
typedef struct
{
    uint8_t source[JDY_ADDRESS_SIZE];      // 源地址
    uint8_t destination[JDY_ADDRESS_SIZE]; // 目标地址
    uint8_t payload[JDY_MAX_PAYLOAD_SIZE]; // 消息体
    size_t payload_len;                    // 消息体长度
} jdy_frame_t;

/* 一个设备对应一个上下文，不能被多个线程无锁并发操作。 */
typedef struct
{
    uint8_t input[JDY_PARSER_BUFFER_SIZE]; // 输入环形缓存
    size_t read_pos;                       // 读指针，指向下一个待处理字节
    size_t write_pos;                      // 写指针，指向下一个可写入字节
    size_t buffered;                       // 环形缓冲区中尚未读取的有效字节数

    jdy_parser_state_t state;        // 当前状态机状态
    uint8_t body[JDY_MAX_BODY_SIZE]; // 当前帧体：源地址、目标地址和消息体
    size_t body_length;              // 长度字段指定的帧体总长度，不含帧头和长度字节
    size_t body_received;            // 当前帧体已接收的字节数
} jdy_parser_t;

/**
 * @brief 初始化或重置上下文，丢弃所有缓存及半包。
 * @param parser 解析器上下文
 * @return JDY_PARSER_OK 表示成功；parser 为空时返回 JDY_PARSER_ERR_ARGUMENT。
 */
jdy_parser_status_t parser_init(jdy_parser_t *parser);

/**
 * @brief 将字节追加到输入缓存，不推进状态机。
 * @param parser 解析器上下文
 * @param data 输入数据
 * @param len 输入数据长度
 * @return JDY_PARSER_OK 表示全部接收；空间不足时返回 JDY_PARSER_ERR_NO_SPACE
 *         且不接收任何字节；参数无效时返回 JDY_PARSER_ERR_ARGUMENT。
 *         len 为 0 时 data 可为空。
 */
jdy_parser_status_t parser_feed(jdy_parser_t *parser,
                                const uint8_t *data,
                                size_t len);

/**
 * @brief 消费缓存并推进状态机，每次最多输出一帧。
 * @param parser 解析器上下文
 * @param out_frame 输出帧
 * @return JDY_PARSER_OK 表示输出一帧；JDY_PARSER_NEED_MORE 表示保留半包
 *         状态且 out_frame 不变；参数无效时返回 JDY_PARSER_ERR_ARGUMENT。
 *         噪声和非法长度会被跳过；parser 与 out_frame 不得重叠。
 */
jdy_parser_status_t parser_next(jdy_parser_t *parser,
                                jdy_frame_t *out_frame);

#endif
