#include "gateway/serial.h"

#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int serial_open(const char *path)
{
    // O_NOCTTY 防止串口被进程意外设为控制终端。
    int fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror("open serial");
        return -1;
    }

    struct termios options;

    if (tcgetattr(fd, &options) < 0)
    {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    // 清除行规程处理，避免二进制帧被改写或等到换行才交给读取端。
    options.c_cflag = 0;
    options.c_oflag = 0;
    options.c_iflag = 0;
    options.c_lflag = 0;

    // 8N1 启用接收 忽略调制解调器控制线
    options.c_cflag &= ~(tcflag_t)(CSIZE | PARENB | CSTOPB);
    options.c_cflag |= CS8 | CREAD | CLOCAL;

#ifdef CRTSCTS
    options.c_cflag &= ~(tcflag_t)CRTSCTS; // 关闭硬件流控
#endif

    // 阻塞模式下至少等到1字节；IO 循环之后会把文件描述符设为非阻塞。
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;

    if (cfsetispeed(&options, B115200) < 0 || cfsetospeed(&options, B115200) < 0 || tcsetattr(fd, TCSANOW, &options) < 0)
    {
        perror("configure serial");
        close(fd);
        return -1;
    }
    return fd;
}

ssize_t serial_read(int fd, void *buffer, size_t cappacity)
{
    return read(fd, buffer, cappacity);
}

ssize_t serial_write(int fd, const void *data, size_t len)
{
    return write(fd, data, len);
}

int serial_close(int fd)
{
    return close(fd);
}
