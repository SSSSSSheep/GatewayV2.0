#ifndef GATEWAY_SERIAL_TX_QUEUE_H
#define GATEWAY_SERIAL_TX_QUEUE_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#define GATEWAY_TX_QUEUE_CAPACITY 16U
#define GATEWAY_TX_FRAME_MAX_SIZE 256U

typedef struct
{
    uint8_t data[GATEWAY_TX_FRAME_MAX_SIZE];
    size_t len;
} gateway_tx_item_t;

typedef struct
{
    pthread_mutex_t mutex;
    int notify_fd;
    size_t head;
    size_t tail;
    size_t count;
    gateway_tx_item_t items[GATEWAY_TX_QUEUE_CAPACITY];
} gateway_serial_tx_queue_t;

typedef enum
{
    GATEWAY_SERIAL_TX_QUEUE_PUSH_OK = 0,
    GATEWAY_SERIAL_TX_QUEUE_PUSH_REJECTED = -1,
    GATEWAY_SERIAL_TX_QUEUE_PUSH_NOTIFY_ERROR = -2
} gateway_serial_tx_queue_push_status_t;

/**
 * @brief 初始化串口发送队列和 eventfd 通知描述符
 * @param queue 待初始化的队列
 * @return 成功返回0；失败返回-1并设置 errno
 */
int gateway_serial_tx_queue_init(gateway_serial_tx_queue_t *queue);

/**
 * @brief 关闭队列通知描述符并销毁互斥锁
 * @param queue 已初始化的队列
 */
void gateway_serial_tx_queue_destroy(gateway_serial_tx_queue_t *queue);

/**
 * @brief 获取供 epoll 监听的 eventfd
 * @param queue 串口发送队列
 * @return 成功返回通知描述符；参数无效时返回-1并设置 errno
 */
int gateway_serial_tx_queue_fd(const gateway_serial_tx_queue_t *queue);

/**
 * @brief 通知 IO 循环检查发送队列
 * @param queue 串口发送队列
 * @return 成功返回0；失败返回-1并设置 errno
 */
int gateway_serial_tx_queue_notify(gateway_serial_tx_queue_t *queue);

/**
 * @brief 将一帧复制到发送队列并通知 IO 循环
 * @param queue 串口发送队列
 * @param data 待发送帧
 * @param len 帧长度
 * @return 通知成功返回0；未入队返回-1；已入队但通知失败返回-2
 */
int gateway_serial_tx_queue_push(gateway_serial_tx_queue_t *queue, const uint8_t *data, size_t len);

/**
 * @brief 从发送队列取出一帧
 * @param queue 串口发送队列
 * @param item 输出帧
 * @return 取到帧返回1，队列为空返回0，出错返回-1并设置 errno
 */
int gateway_serial_tx_queue_pop(gateway_serial_tx_queue_t *queue, gateway_tx_item_t *item);

#endif // !GATEWAY_SERIAL_TX_QUEUE_H
