#include "gateway/serial_tx_queue.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>

int gateway_serial_tx_queue_init(gateway_serial_tx_queue_t *queue)
{
    int result;

    if (queue == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    memset(queue, 0, sizeof(*queue));
    queue->notify_fd = -1;

    result = pthread_mutex_init(&queue->mutex, NULL);
    if (result != 0)
    {
        errno = result;
        return -1;
    }

    queue->notify_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (queue->notify_fd < 0)
    {
        result = errno;
        pthread_mutex_destroy(&queue->mutex);
        errno = result;
        return -1;
    }
    return 0;
}

void gateway_serial_tx_queue_destroy(gateway_serial_tx_queue_t *queue)
{
    if (queue == NULL)
    {
        return;
    }

    if (queue->notify_fd >= 0)
    {
        close(queue->notify_fd);
        queue->notify_fd = -1;
    }

    pthread_mutex_destroy(&queue->mutex);
}

int gateway_serial_tx_queue_fd(const gateway_serial_tx_queue_t *queue)
{
    if (queue == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    return queue->notify_fd;
}

int gateway_serial_tx_queue_push(gateway_serial_tx_queue_t *queue, const uint8_t *data, size_t len)
{
    uint64_t notification = 1;
    ssize_t written;
    int result;

    if (queue == NULL || data == NULL || len == 0U || len > GATEWAY_TX_FRAME_MAX_SIZE)
    {
        errno = EINVAL;
        return -1;
    }

    result = pthread_mutex_lock(&queue->mutex);
    if (result != 0)
    {
        errno = result;
        return -1;
    }

    if (queue->count == GATEWAY_TX_QUEUE_CAPACITY)
    {
        // 队列满时不覆盖旧帧，也不把新帧算作已入队。
        pthread_mutex_unlock(&queue->mutex);
        errno = ENOSPC;
        return -1;
    }

    memcpy(queue->items[queue->tail].data, data, len);
    queue->items[queue->tail].len = len;
    queue->tail = (queue->tail + 1) % GATEWAY_TX_QUEUE_CAPACITY;
    queue->count++;

    do
    {
        written = write(queue->notify_fd, &notification, sizeof(notification));
    } while (written < 0 && errno == EINTR); // 信号打断 eventfd 写入时重试。

    // 计数器已满时 eventfd 返回 EAGAIN，但通知 fd 仍可读，不必撤销队列项。
    if (written < 0 && errno != EAGAIN)
    {
        result = errno;
        queue->tail = (queue->tail + GATEWAY_TX_QUEUE_CAPACITY - 1U) % GATEWAY_TX_QUEUE_CAPACITY;
        queue->count--;
        pthread_mutex_unlock(&queue->mutex);

        errno = result;
        return -1;
    }

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int gateway_serial_tx_queue_pop(gateway_serial_tx_queue_t *queue, gateway_tx_item_t *item)
{
    int result;

    if (queue == NULL || item == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    result = pthread_mutex_lock(&queue->mutex);
    if (result != 0)
    {
        errno = result;
        return -1;
    }

    if (queue->count == 0U)
    {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }

    *item = queue->items[queue->head];
    queue->head = (queue->head + 1U) % GATEWAY_TX_QUEUE_CAPACITY;
    queue->count--;

    pthread_mutex_unlock(&queue->mutex);
    return 1;
}
