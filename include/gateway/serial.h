#ifndef GATEWAY_SERIAL_H
#define GATEWAY_SERIAL_H

#include <stddef.h>
#include <sys/types.h>

/**
 * @brief 打开串口设备并配置为 115200 8N1
 * @param path 串口设备路径
 * @return 成功返回文件描述符，失败返回 -1
 */
int serial_open(const char *path);

/**
 * @brief 从串口设备读取数据；是否阻塞由文件描述符标志决定
 * @param fd 文件描述符
 * @param buffer 缓冲区
 * @param cappacity 缓冲区大小
 * @return 实际读取的字节数，失败返回 -1
 */
ssize_t serial_read(int fd, void *buffer, size_t cappacity);

/**
 * @brief 向串口设备写入数据(单次写入)
 * @param fd 文件描述符
 * @param data 数据
 * @param len 数据长度
 * @return 实际写入的字节数，失败返回 -1
 */
ssize_t serial_write(int fd, const void *data, size_t len);

/**
 * @brief 关闭串口设备
 * @param fd 文件描述符
 * @return 成功返回 0，失败返回 -1
 */
int serial_close(int fd);

#endif // !GATEWAY_SERIAL_H
