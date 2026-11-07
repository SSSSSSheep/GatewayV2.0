#include "gateway/serial_tx_queue.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define CHECK(condition)                              \
    do                                                \
    {                                                 \
        if (!(condition))                             \
        {                                             \
            fprintf(stderr, "FAIL %s:%d: %s\n",       \
                    __FILE__, __LINE__, #condition);  \
            return 1;                                 \
        }                                             \
    } while (0)

#define CONCURRENT_ITEMS 100000U

typedef struct
{
    gateway_serial_tx_queue_t *queue;
    int failed;
} queue_thread_args_t;

static void encode_sequence(uint32_t sequence, uint8_t data[4])
{
    data[0] = (uint8_t)(sequence >> 24);
    data[1] = (uint8_t)(sequence >> 16);
    data[2] = (uint8_t)(sequence >> 8);
    data[3] = (uint8_t)sequence;
}

static uint32_t decode_sequence(const uint8_t data[4])
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static void *queue_producer(void *arg)
{
    queue_thread_args_t *args = arg;

    for (uint32_t sequence = 0; sequence < CONCURRENT_ITEMS; ++sequence)
    {
        uint8_t data[4];
        int result;
        encode_sequence(sequence, data);

        do
        {
            result = gateway_serial_tx_queue_push(args->queue, data, sizeof(data));
            if (result == GATEWAY_SERIAL_TX_QUEUE_PUSH_REJECTED && errno == ENOSPC)
            {
                sched_yield();
            }
            else if (result != GATEWAY_SERIAL_TX_QUEUE_PUSH_OK)
            {
                args->failed = 1;
                return NULL;
            }
        } while (result == GATEWAY_SERIAL_TX_QUEUE_PUSH_REJECTED);

    }

    return NULL;
}

static void *queue_consumer(void *arg)
{
    queue_thread_args_t *args = arg;

    for (uint32_t expected = 0; expected < CONCURRENT_ITEMS; ++expected)
    {
        gateway_tx_item_t item;
        int result;

        do
        {
            result = gateway_serial_tx_queue_pop(args->queue, &item);
            if (result == 0)
            {
                sched_yield();
            }
        } while (result == 0);

        if (result != 1 || item.len != 4U ||
            decode_sequence(item.data) != expected)
        {
            args->failed = 1;
            return NULL;
        }
    }

    return NULL;
}

int main(void)
{
    gateway_serial_tx_queue_t queue;
    CHECK(gateway_serial_tx_queue_init(&queue) == 0);

    for (uint32_t sequence = 0; sequence < GATEWAY_TX_QUEUE_CAPACITY; ++sequence)
    {
        uint8_t data[4];
        encode_sequence(sequence, data);
        CHECK(gateway_serial_tx_queue_push(&queue, data, sizeof(data)) ==
              GATEWAY_SERIAL_TX_QUEUE_PUSH_OK);
    }

    uint8_t extra[] = {0xFF};
    errno = 0;
    CHECK(gateway_serial_tx_queue_push(&queue, extra, sizeof(extra)) ==
          GATEWAY_SERIAL_TX_QUEUE_PUSH_REJECTED);
    CHECK(errno == ENOSPC);

    uint64_t notifications = 0;
    CHECK(read(queue.notify_fd, &notifications, sizeof(notifications)) ==
          (ssize_t)sizeof(notifications));
    CHECK(notifications == GATEWAY_TX_QUEUE_CAPACITY);

    for (uint32_t expected = 0; expected < GATEWAY_TX_QUEUE_CAPACITY; ++expected)
    {
        gateway_tx_item_t item;
        CHECK(gateway_serial_tx_queue_pop(&queue, &item) == 1);
        CHECK(item.len == 4U);
        CHECK(decode_sequence(item.data) == expected);
    }
    CHECK(gateway_serial_tx_queue_pop(&queue, &(gateway_tx_item_t){0}) == 0);

    queue_thread_args_t producer_args = {.queue = &queue, .failed = 0};
    queue_thread_args_t consumer_args = {.queue = &queue, .failed = 0};
    pthread_t producer;
    pthread_t consumer;

    CHECK(pthread_create(&consumer, NULL, queue_consumer, &consumer_args) == 0);
    CHECK(pthread_create(&producer, NULL, queue_producer, &producer_args) == 0);
    CHECK(pthread_join(producer, NULL) == 0);
    CHECK(pthread_join(consumer, NULL) == 0);
    CHECK(producer_args.failed == 0);
    CHECK(consumer_args.failed == 0);

    gateway_serial_tx_queue_destroy(&queue);
    puts("serial_tx_queue tests passed");
    return 0;
}
