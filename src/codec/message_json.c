#include "gateway/message_json.h"
#include "gateway/hex_decode.h"
#include "cJSON.h"

#include <string.h>

/*
 * cJSON 用 NUL 结尾的 C 字符串保存文本。
 * 拒绝转义的 U+0000，避免 strlen() 忽略其后的内容。
 */
static int contains_escaped_nul(const char *text, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        // 跳过非反斜杠字符。
        if (text[i] != '\\')
        {
            continue;
        }

        // 跳过转义的反斜杠。
        if (len - i >= 6 &&
            memcmp(text + i, "\\u0000", 6) == 0)
        {
            return 1;
        }

        /* 跳过被转义的字符，包括转义反斜杠。 */
        if (i + 1 < len)
        {
            ++i;
        }
    }

    return 0;
}

/*
 * message_json_decode 数据流转图解：
 *
 * 【输入】{"conn_type":1,"id":"0002","msg":"31"}
 *         ↓
 * 【解析】conn_type       | id              | msg
 *         数值 1         | 字符串 "0002"   | 字符串 "31"
 *         ↓
 * 【校验】必须为数值1    | 必须为4个HEX字符 | 必须为1～12字节的HEX文本
 *         ↓
 * 【转换】保留数值1      | hex_decode()    | hex_decode()
 *         ↓
 * 【结果】1              | 0x00 0x02       | 0x31
 *         ↓
 * 【输出】gateway_message_t
 *         conn_type = 1
 *         id[0]     = 0x00
 *         id[1]     = 0x02
 *         msg_len   = 1
 *         msg[0]    = 0x31，其余msg元素为0
 *         ↓
 * 【释放】cJSON_Delete(root)，释放解析树
 * 【返回】MESSAGE_JSON_OK
 *
 * 错误示例：
 *   {"id":"0002"}                    → MESSAGE_JSON_ERR_FIELD
 *   conn_type为字符串"1"             → MESSAGE_JSON_ERR_TYPE
 *   msg为字符串"3G"                  → MESSAGE_JSON_ERR_HEX
 *   解析树创建成功后，失败路径统一释放；输出结构体保持不变。
 */
message_json_status_t message_json_to_decode(const void *payload,
                                             size_t payload_len,
                                             gateway_message_t *out_msg)
{
    // 参数校验
    if (payload == NULL || out_msg == NULL || payload_len == 0)
    {
        return MESSAGE_JSON_ERR_ARGUMENT;
    }

    // JSON 文本长度限制
    if (payload_len > GATEWAY_MAX_JSON_SIZE)
    {
        return MESSAGE_JSON_ERR_TOO_LARGE;
    }

    // JSON 文本包含 NUL 字符
    const char *input = (const char *)payload;

    if (memchr(input, '\0', payload_len) != NULL ||
        contains_escaped_nul(input, payload_len))
    {
        return MESSAGE_JSON_ERR_JSON;
    }

    /*
     * 输入长度已受限制，所以这里可以安全复制并补 NUL。
     * 不直接把 MQTT payload 当成普通 C 字符串使用。
     */
    char text[GATEWAY_MAX_JSON_SIZE + 1];
    memcpy(text, input, payload_len);
    text[payload_len] = '\0';

    /*
     * 要求解析到字符串结尾，拒绝 JSON 后面的垃圾内容。
     */
    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(
        text,
        payload_len + 1,
        &parse_end,
        1);

    if (root == NULL)
    {
        return MESSAGE_JSON_ERR_JSON;
    }

    message_json_status_t result = MESSAGE_JSON_ERR_JSON;
    gateway_message_t decoded = {0};

    const cJSON *conn = NULL;
    const cJSON *id = NULL;
    const cJSON *msg = NULL;
    const cJSON *item = NULL;

    if (!cJSON_IsObject(root))
    {
        goto cleanup;
    }

    result = MESSAGE_JSON_ERR_FIELD;

    /*
     * 遍历字段，既检查未知字段，也检查重复字段。
     */
    cJSON_ArrayForEach(item, root)
    {
        if (item->string == NULL)
        {
            goto cleanup;
        }

        if (strcmp(item->string, "conn_type") == 0)
        {
            if (conn != NULL)
            {
                goto cleanup;
            }
            conn = item;
        }
        else if (strcmp(item->string, "id") == 0)
        {
            if (id != NULL)
            {
                goto cleanup;
            }
            id = item;
        }
        else if (strcmp(item->string, "msg") == 0)
        {
            if (msg != NULL)
            {
                goto cleanup;
            }
            msg = item;
        }
        else
        {
            goto cleanup;
        }
    }

    if (conn == NULL || id == NULL || msg == NULL)
    {
        goto cleanup;
    }

    result = MESSAGE_JSON_ERR_TYPE;

    if (!cJSON_IsNumber(conn) ||
        !cJSON_IsString(id) || id->valuestring == NULL ||
        !cJSON_IsString(msg) || msg->valuestring == NULL)
    {
        goto cleanup;
    }

    result = MESSAGE_JSON_ERR_RANGE;

    /*
     * 不能仅检查 valueint：
     * 例如 1.5 转成整数后可能为 1。
     */
    if (conn->valuedouble != 1.0)
    {
        goto cleanup;
    }

    const size_t id_hex_len = strlen(id->valuestring);
    const size_t msg_hex_len = strlen(msg->valuestring);

    if (id_hex_len != GATEWAY_ID_SIZE * 2 ||
        msg_hex_len == 0 ||
        msg_hex_len > GATEWAY_MAX_MSG_SIZE * 2)
    {
        goto cleanup;
    }

    result = MESSAGE_JSON_ERR_HEX;

    size_t id_len = 0;

    if (hex_decode(id->valuestring,
                   id_hex_len,
                   decoded.id,
                   sizeof(decoded.id),
                   &id_len) != HEX_DECODE_OK ||
        id_len != GATEWAY_ID_SIZE)
    {
        goto cleanup;
    }

    if (hex_decode(msg->valuestring,
                   msg_hex_len,
                   decoded.msg,
                   sizeof(decoded.msg),
                   &decoded.msg_len) != HEX_DECODE_OK)
    {
        goto cleanup;
    }

    decoded.conn_type = 1;

    /* 全部校验成功后，才修改调用者的输出。 */
    *out_msg = decoded;
    result = MESSAGE_JSON_OK;

cleanup:
    cJSON_Delete(root);
    return result;
}