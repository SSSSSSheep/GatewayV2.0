#include "gateway/jdy_parser.h"
#include "gateway/io_loop.h"
#include "gateway/message_json.h"
#include "gateway/mqtt.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>

#define IO_LOOP_MAX_EVENTS 1  // 最多处理一个事件
#define IO_LOOP_READ_SIZE 128 // 每次读取的最大字节数

int io_loop_run(int serial_fd)
{
    // 获取当前fd文件状态标志
    int flags = fcntl(serial_fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("fcntl F_GETFL failed");
        return -1;
    }

    // 设置串口为非阻塞模式
    if (fcntl(serial_fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("fcntl F_SETFL failed");
        return -1;
    }

    // 创建epoll实例
    int epoll_fd = epoll_create(1);
    if (epoll_fd < 0)
    {
        perror("epoll_create failed");
        return -1;
    }

    // 创建epoll事件
    struct epoll_event event;
    // 设置触发条件为可读
    event.events = EPOLLIN;
    // 设置文件描述符
    event.data.fd = serial_fd;

    // 将文件描述符添加到epoll实例中
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serial_fd, &event) < 0)
    {
        perror("epoll_ctl failed");
        return -1;
    }

    // 初始化解析器
    jdy_parser_t parser;
    if (parser_init(&parser) != JDY_PARSER_OK)
    {
        fprintf(stderr, "解析器初始化失败\n");
        close(epoll_fd);
        return -1;
    }

    printf("循环事件已启动，等待串口数据……\n");

    // 循环处理事件
    while (1)
    {
        // 创建事件数组
        struct epoll_event events[IO_LOOP_MAX_EVENTS];
        // 等待事件发生
        int event_count = epoll_wait(epoll_fd, events, IO_LOOP_MAX_EVENTS, -1);

        // 报错看errno
        if (event_count < 0)
        {
            if (errno == EINTR)
            {
                // 被信号中断 重试
                continue;
            }
            perror("epoll_wait failed");
            close(epoll_fd);
            return -1;
        }

        if ((events[0].events & EPOLLIN) == 0)
        {
            // 没有事件发生 继续
            continue;
        }

        // 读取串口数据
        while (1)
        {
            // 读缓冲区
            uint8_t read_buffer[IO_LOOP_READ_SIZE];
            // 读缓冲区指针
            ssize_t read_len = read(serial_fd, read_buffer, IO_LOOP_READ_SIZE);

            if (read_len < 0)
            {
                // errno = EAGIN 或 EWOULDBLOCK，说明不是read失败
                // 而是read在以非阻塞方式读一个设备文件/网络文件，并且文件无数据
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }

                if (errno == EINTR)
                {
                    // 被信号中断 重试
                    continue;
                }

                perror("read serial");
                close(epoll_fd);
                return -1;
            }

            // 读到了EOF
            if (read_len == 0)
            {
                break;
            }

            // 将数据喂给解析器
            jdy_parser_status_t feed_status = parser_feed(&parser, read_buffer, (size_t)read_len);

            if (feed_status != JDY_PARSER_OK)
            {
                fprintf(stderr, "解析器输入缓存无法接收数据，状态码：%d\n", feed_status);
                close(epoll_fd);
                return -1;
            }

            // 检查缓存中是否还有完整的一帧
            while (1)
            {
                jdy_frame_t frame;
                jdy_parser_status_t parse_status = parser_next(&parser, &frame);

                if (parse_status == JDY_PARSER_OK)
                {
                    gateway_message_t message = {0};
                    const size_t message_header_len =
                        JDY_ADDRESS_SIZE + 1U;

                    message.conn_type = 1;
                    memcpy(message.id, frame.source, JDY_ADDRESS_SIZE);

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
                    memcpy(message.msg,
                           frame.payload + message_header_len,
                           message.msg_len);

                    if (gateway_mqtt_publish_up(&message) != 0)
                    {
                        fprintf(stderr, "串口帧上行发布失败\n");
                    }

                    printf("成功解析出一帧！消息体长度: %zu\n",
                           message.msg_len);
                    continue;
                }

                if (parse_status == JDY_PARSER_NEED_MORE)
                {
                    break; // 半包状态保留，等待更多串口数据
                }

                fprintf(stderr, "帧解析失败，状态码: %d\n", parse_status);

                close(epoll_fd);
                return -1;
            }
        }
    }
}