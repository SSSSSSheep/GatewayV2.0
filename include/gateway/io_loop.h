#ifndef GATEWAY_IO_LOOP_H
#define GATEWAY_IO_LOOP_H

/**
 * @brief 启动IO循环
 * @param serial_fd 串口文件描述符
 * @return 0表示成功，-1表示失败
 */
int io_loop_run(int serial_fd);

#endif