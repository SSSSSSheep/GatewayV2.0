#include "gateway/jdy_parser.h"
#include "gateway/io_loop.h"
#include "gateway/message_json.h"
#include "gateway/mqtt.h"
#include "gateway/serial_tx_queue.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define IO_LOOP_MAX_EVENTS 2
#define IO_LOOP_READ_SIZE 128

typedef struct io_loop_ctx io_loop_ctx_t;

typedef enum
{
    IO_LOOP_WATCH_SERIAL,
    IO_LOOP_WATCH_TX_NOTIFY
} io_loop_watch_kind_t;

typedef struct
{
    io_loop_ctx_t *ctx;
    io_loop_watch_kind_t kind;
} io_loop_watch_t;

struct io_loop_ctx
{
    int serial_fd;
    int epoll_fd;
    int notify_fd;
    struct epoll_event serial_event;
    gateway_serial_tx_queue_t *tx_queue;
    gateway_tx_item_t pending_item;
    size_t pending_offset;
    int pending_valid;
    jdy_parser_t parser;
    io_loop_watch_t serial_watch;
    io_loop_watch_t notify_watch;
};

/**
 * @brief 尝试继续写入当前串口帧
 * @param ctx IO循环上下文
 * @param data 待写入的数据
 * @param len 待写入的数据长度
 * @param offset 已写入的字节数，函数会更新该值
 * @return 0表示本轮没有错误，-1表示写入失败；是否写完由offset判断
 */
static int serial_write_full(
    io_loop_ctx_t *ctx,
    const uint8_t *data,
    size_t len,
    size_t *offset)
{
    while (*offset < len)
    {
        ssize_t written = write(
            ctx->serial_fd,
            data + *offset,
            len - *offset);

        if (written > 0)
        {
            *offset += (size_t)written;
            continue;
        }

        if (written == 0)
        {
            errno = EIO;
            return -1;
        }

        if (errno == EINTR)
        {
            // 信号只打断了本次写入，已写字节数由 offset 保留，继续尝试。
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // 串口暂时不可写；保留 offset，等 EPOLLOUT 到来后续写。
            return 0;
        }

        return -1;
    }

    return 0;
}

/**
 * @brief 修改串口文件描述符的epoll监听事件
 * @param ctx IO循环上下文
 * @param events 新的监听事件
 * @return 0表示成功，-1表示失败
 */
static int update_serial_events(io_loop_ctx_t *ctx, uint32_t events)
{
    ctx->serial_event.events = events;

    if (epoll_ctl(
            ctx->epoll_fd,
            EPOLL_CTL_MOD,
            ctx->serial_fd,
            &ctx->serial_event) < 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief 读取并处理串口接收数据
 * @param ctx IO循环上下文
 * @return 0表示处理成功，-1表示失败
 */
static int process_serial_input(io_loop_ctx_t *ctx)
{
    while (1)
    {
        uint8_t read_buffer[IO_LOOP_READ_SIZE];
        ssize_t read_len = read(
            ctx->serial_fd,
            read_buffer,
            sizeof(read_buffer));

        if (read_len < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                fprintf(stderr, "[诊断] 串口 read 返回 EAGAIN\n");
                // 当前内核接收缓冲已读空，回到 epoll 等下一批字节。
                return 0;
            }

            if (errno == EINTR)
            {
                // 读取被信号打断，不丢弃解析状态，继续尝试。
                continue;
            }

            perror("read failed");
            return -1;
        }

        if (read_len == 0)
        {
            return 0;
        }

        fprintf(stderr, "[诊断] 串口 read 到 %zd 字节\n", read_len);

        jdy_parser_status_t feed_status = parser_feed(
            &ctx->parser,
            read_buffer,
            (size_t)read_len);

        if (feed_status != JDY_PARSER_OK)
        {
            fprintf(stderr, "解析器输入缓存无法接收数据，状态码：%d\n",
                    feed_status);
            return -1;
        }

        while (1)
        {
            jdy_frame_t frame;
            jdy_parser_status_t parse_status =
                parser_next(&ctx->parser, &frame);

            if (parse_status == JDY_PARSER_OK)
            {
                gateway_message_t message = {0};
                const size_t message_header_len = JDY_ADDRESS_SIZE + 1U;

                message.conn_type = 1;
                memcpy(message.id, frame.source, JDY_ADDRESS_SIZE);

                // 先核对帧内地址和长度字段，再复制消息体，避免格式错误导致越界读取。
                if (frame.payload_len <= message_header_len ||
                    memcmp(frame.payload, frame.source,
                           JDY_ADDRESS_SIZE) != 0 ||
                    (size_t)frame.payload[JDY_ADDRESS_SIZE] !=
                        frame.payload_len - message_header_len ||
                    frame.payload_len - message_header_len >
                        sizeof(message.msg))
                {
                    fprintf(stderr, "STM32消息格式错误\n");
                    continue;
                }

                message.msg_len =
                    frame.payload_len - message_header_len;
                memcpy(
                    message.msg,
                    frame.payload + message_header_len,
                    message.msg_len);
                fprintf(stderr, "[诊断] 开始 MQTT 上行发布\n");
                if (gateway_mqtt_publish_up(&message) != 0)
                {
                    fprintf(stderr, "串口帧上行发布失败\n");
                }
                fprintf(stderr, "[诊断] MQTT 上行发布调用已返回\n");
                printf("成功解析出一帧！消息体长度: %zu\n",
                       message.msg_len);
                continue;
            }

            if (parse_status == JDY_PARSER_NEED_MORE)
            {
                break;
            }

            fprintf(stderr, "帧解析失败，状态码: %d\n", parse_status);
            return -1;
        }
    }
}

/**
 * @brief 读取串口发送队列的eventfd通知
 * @param ctx IO循环上下文
 * @return 0表示读取成功，-1表示失败
 */
static int consume_tx_notification(io_loop_ctx_t *ctx)
{
    uint64_t notification_count;
    ssize_t bytes_read;

    do
    {
        bytes_read = read(
            ctx->notify_fd,
            &notification_count,
            sizeof(notification_count));
    } while (bytes_read < 0 && errno == EINTR); // 通知读取被信号打断时重试。

    if (bytes_read == (ssize_t)sizeof(notification_count))
    {
        return 0;
    }

    if (bytes_read < 0 &&
        (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        // 当前没有待处理的 eventfd 计数；调用方仍会检查队列。
        return 0;
    }

    if (bytes_read >= 0)
    {
        errno = EIO;
    }

    return -1;
}

/**
 * @brief 处理发送队列并续写未完成的串口帧
 * @param ctx IO循环上下文
 * @return 0表示处理成功，-1表示失败
 */
static int process_tx_queue(io_loop_ctx_t *ctx)
{
    while (1)
    {
        if (ctx->pending_valid == 0)
        {
            int pop_result = gateway_serial_tx_queue_pop(
                ctx->tx_queue,
                &ctx->pending_item);

            if (pop_result < 0)
            {
                perror("读取串口发送队列失败");
                return -1;
            }

            if (pop_result == 0)
            {
                return 0;
            }

            ctx->pending_offset = 0U;
            ctx->pending_valid = 1;
        }

        if (serial_write_full(
                ctx,
                ctx->pending_item.data,
                ctx->pending_item.len,
                &ctx->pending_offset) != 0)
        {
            perror("写入串口失败");
            return -1;
        }

        if (ctx->pending_offset < ctx->pending_item.len)
        {
            if (update_serial_events(
                    ctx,
                    EPOLLIN | EPOLLOUT) < 0)
            {
                perror("启用串口EPOLLOUT监听失败");
                return -1;
            }

            return 0;
        }

        printf("串口写入：%zu/%zu 字节\n",
               ctx->pending_offset,
               ctx->pending_item.len);

        if (update_serial_events(ctx, EPOLLIN) < 0)
        {
            perror("关闭串口EPOLLOUT监听失败");
            return -1;
        }

        ctx->pending_offset = 0U;
        ctx->pending_valid = 0;
    }
}

/**
 * @brief 启动IO循环
 * @param serial_fd 串口文件描述符
 * @param tx_queue 串口发送队列
 * @return 0表示成功，-1表示失败
 */
int io_loop_run(int serial_fd, gateway_serial_tx_queue_t *tx_queue)
{
    if (serial_fd < 0 || tx_queue == NULL)
    {
        errno = EINVAL;
        perror("IO循环参数无效");
        return -1;
    }

    io_loop_ctx_t ctx = {
        .serial_fd = serial_fd,
        .epoll_fd = -1,
        .notify_fd = -1,
        .tx_queue = tx_queue,
    };

    int flags = fcntl(ctx.serial_fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("fcntl F_GETFL failed");
        return -1;
    }

    if (fcntl(ctx.serial_fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("fcntl F_SETFL failed");
        return -1;
    }

    ctx.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx.epoll_fd < 0)
    {
        perror("epoll_create1 failed");
        return -1;
    }

    ctx.notify_fd = gateway_serial_tx_queue_fd(ctx.tx_queue);
    if (ctx.notify_fd < 0)
    {
        perror("获取发送队列通知 fd 失败");
        close(ctx.epoll_fd);
        return -1;
    }

    if (parser_init(&ctx.parser) != JDY_PARSER_OK)
    {
        fprintf(stderr, "解析器初始化失败\n");
        close(ctx.epoll_fd);
        return -1;
    }

    ctx.serial_watch.ctx = &ctx;
    ctx.serial_watch.kind = IO_LOOP_WATCH_SERIAL;

    ctx.notify_watch.ctx = &ctx;
    ctx.notify_watch.kind = IO_LOOP_WATCH_TX_NOTIFY;

    ctx.serial_event.events = EPOLLIN;
    ctx.serial_event.data.ptr = &ctx.serial_watch;

    if (epoll_ctl(
            ctx.epoll_fd,
            EPOLL_CTL_ADD,
            ctx.serial_fd,
            &ctx.serial_event) < 0)
    {
        perror("epoll_ctl add serial fd failed");
        close(ctx.epoll_fd);
        return -1;
    }

    struct epoll_event tx_event = {0};
    tx_event.events = EPOLLIN;
    tx_event.data.ptr = &ctx.notify_watch;

    if (epoll_ctl(
            ctx.epoll_fd,
            EPOLL_CTL_ADD,
            ctx.notify_fd,
            &tx_event) < 0)
    {
        perror("epoll_ctl add notify fd failed");
        close(ctx.epoll_fd);
        return -1;
    }

    printf("循环事件已启动，等待串口数据……\n");

    while (1)
    {
        struct epoll_event events[IO_LOOP_MAX_EVENTS];
        int event_count = epoll_wait(
            ctx.epoll_fd,
            events,
            IO_LOOP_MAX_EVENTS,
            -1);

        if (event_count < 0)
        {
            if (errno == EINTR)
            {
                // 信号中断等待不代表 IO 失败，重新等待事件。
                continue;
            }

            perror("epoll_wait failed");
            close(ctx.epoll_fd);
            return -1;
        }

        for (int event_index = 0;
             event_index < event_count;
             event_index++)
        {
            uint32_t ready_events = events[event_index].events;
            io_loop_watch_t *watch = events[event_index].data.ptr;

            if ((ready_events & (EPOLLERR | EPOLLHUP)) != 0)
            {
                fprintf(stderr, "epoll检测到文件描述符错误\n");
                close(ctx.epoll_fd);
                return -1;
            }

            if (watch == NULL || watch->ctx != &ctx)
            {
                fprintf(stderr, "epoll返回了无效事件上下文\n");
                close(ctx.epoll_fd);
                return -1;
            }

            fprintf(stderr, "[诊断] epoll返回 kind=%d events=0x%x\n",
                    (int)watch->kind, ready_events);
            if (watch->kind == IO_LOOP_WATCH_TX_NOTIFY)
            {
                if ((ready_events & EPOLLIN) == 0)
                {
                    continue;
                }

                if (consume_tx_notification(&ctx) != 0)
                {
                    perror("读取串口发送队列通知失败");
                    close(ctx.epoll_fd);
                    return -1;
                }

                if (process_tx_queue(&ctx) != 0)
                {
                    close(ctx.epoll_fd);
                    return -1;
                }

                continue;
            }

            if (watch->kind == IO_LOOP_WATCH_SERIAL)
            {
                if ((ready_events & EPOLLIN) != 0 &&
                    process_serial_input(&ctx) != 0)
                {
                    close(ctx.epoll_fd);
                    return -1;
                }

                if ((ready_events & EPOLLOUT) != 0 &&
                    process_tx_queue(&ctx) != 0)
                {
                    close(ctx.epoll_fd);
                    return -1;
                }
            }
        }
    }
}
