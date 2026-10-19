#include "gateway/jdy_at.h"

#include <string.h>

/*
  内部结构体（gateway_message_t）示例:
    conn_type: 1
    id:        00 02
    msg_len:   1
    msg:       31

  输出蓝牙数据格式（本例16字节）:
 [0~6]     [7]   [8~9]   [10~11] [12]   [13]     [14~15]
 AT+MESH   00    00 02   01 01   01     31       0D 0A
 头部      CMD   目标    源地址  长度   业务数据   CRLF

 源地址、长度为自定义业务头；总长度为15 + msg_len。
 */
jdy_at_status_t jdy_at_build(const gateway_message_t *msg,
                             uint8_t *out,
                             size_t out_capacity,
                             size_t *out_len)
{
    if (out_len == NULL)
    {
        return JDY_AT_ERR_ARGUMENT;
    }

    *out_len = 0;

    if (msg == NULL || out == NULL)
    {
        return JDY_AT_ERR_ARGUMENT;
    }

    if (msg->conn_type != 1)
    {
        return JDY_AT_ERR_TYPE;
    }

    /*
     * 先限制长度，再计算所需容量、读取 msg[]。
     * 不能先复制再检查。
     */
    if (msg->msg_len == 0 ||
        msg->msg_len > sizeof(msg->msg))
    {
        return JDY_AT_ERR_LENGTH;
    }

    const size_t required =
        JDY_AT_FIXED_SIZE + msg->msg_len;

    if (required > out_capacity)
    {
        return JDY_AT_ERR_NO_SPACE;
    }

    /*
     * 此处所有参数与长度检查均已完成。
     * 后续操作不会再因参数错误提前退出。
     */
    size_t offset = 0;

    /* 下标0～6：只复制七个字符。 */
    memcpy(out + offset, "AT+MESH", 7);
    offset += 7;

    /* 下标7：明确写入协议参数00。 */
    out[offset++] = 0x00;

    /* 下标8～9：目标地址。 */
    memcpy(out + offset, msg->id, sizeof(msg->id));
    offset += sizeof(msg->id);

    /* 下标10～11：自定义业务源地址0101。 */
    out[offset++] = 0x01;
    out[offset++] = 0x01;

    /* 下标12：自定义业务数据长度。 */
    out[offset++] = (uint8_t)msg->msg_len;

    /* 下标13起：业务数据，允许包含00。 */
    memcpy(out + offset, msg->msg, msg->msg_len);
    offset += msg->msg_len;

    out[offset++] = '\r';
    out[offset++] = '\n';

    *out_len = offset;
    return JDY_AT_OK;
}