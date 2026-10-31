#ifndef GATEWAY_IO_LOOP_H
#define GATEWAY_IO_LOOP_H

#include "gateway/serial_tx_queue.h"

/**
 * @brief 启动IO循环
 * @param serial_fd 串口文件描述符
 * @param tx_queue 串口发送队列
 * @return 0表示成功，-1表示失败
 */
int io_loop_run(int serial_fd, gateway_serial_tx_queue_t *tx_queue);

#endif