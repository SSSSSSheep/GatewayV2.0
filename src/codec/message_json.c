#include "gateway/message_json.h"
#include "gateway/hex_decode.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>

/*
 * cJSON 用 NUL 结尾的 C 字符串保存文本。
 * 拒绝转义的 U+0000，避免 strlen() 忽略其后的内容。
 */
static int contains_escaped_nul(const char *text, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (text[i] != '\\')
        {
            continue;
        }

        if (len - i >= 6 &&
            memcmp(text + i, "\\u0000", 6) == 0)
        {
            return 1;
        }

        /* 跳过转义对，避免把转义反斜杠后面的文本误判成 NUL 转义。 */
        if (i + 1 < len)
        {
            i++;
        }
    }

    return 0;
}

message_json_status_t message_json_to_decode(const void *payload,
                                             size_t payload_len,
                                             gateway_message_t *out_msg)
{
    if (payload == NULL || out_msg == NULL || payload_len == 0)
    {
        return MESSAGE_JSON_ERR_ARGUMENT;
    }

    if (payload_len > GATEWAY_MAX_JSON_SIZE)
    {
        return MESSAGE_JSON_ERR_TOO_LARGE;
    }

    const char *input = (const char *)payload;

    if (memchr(input, '\0', payload_len) != NULL ||
        contains_escaped_nul(input, payload_len))
    {
        return MESSAGE_JSON_ERR_JSON;
    }

    /*
     * 输入长度已受限制，所以这里可以安全复制并补 NUL。
     * MQTT 消息体不保证以 NUL 结尾，不能直接当普通 C 字符串读取。
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

/*
 * 输入：
 *   conn_type = 1
 *   id        = 23 23
 *   msg_len   = 3
 *   msg       = 41 42 43
 *
 * 输出：
 *   {"conn_type":1,"id":"2323","msg":"414243"}
 */
message_json_status_t message_json_to_encode(const gateway_message_t *message, char *out_json, size_t out_capacity, size_t *out_len)
{
    char encoded[GATEWAY_MAX_JSON_SIZE] = {0};
    size_t offset = 0;
    int written = 0;

    if (message == NULL || out_json == NULL || out_len == NULL)
    {
        return MESSAGE_JSON_ERR_ARGUMENT;
    }

    if (message->conn_type != 1)
    {
        return MESSAGE_JSON_ERR_TYPE;
    }

    // 消息体不能为空，也不能超过协议上限。
    if (message->msg_len == 0 || message->msg_len > GATEWAY_MAX_MSG_SIZE)
    {
        return MESSAGE_JSON_ERR_RANGE;
    }

    *out_len = 0;

    written = snprintf(encoded + offset, sizeof(encoded) - offset,
                       "{\"conn_type\":%u,\"id\":\"%02X%02X\",\"msg\":\"",
                       (unsigned int)message->conn_type,
                       (unsigned int)message->id[0],
                       (unsigned int)message->id[1]);

    // snprintf 返回所需长度；超过剩余空间就拒绝截断结果。
    if (written < 0 || (size_t)written >= sizeof(encoded) - offset)
    {
        return MESSAGE_JSON_ERR_JSON;
    }

    offset += (size_t)written;

    // 消息体按字节编码成两位大写 HEX，二进制零也能完整表示。
    for (size_t i = 0; i < message->msg_len; i++)
    {
        encoded[offset] = "0123456789ABCDEF"[message->msg[i] >> 4];
        encoded[offset + 1U] = "0123456789ABCDEF"[message->msg[i] & 0x0FU];
        offset += 2U;
    }

    written = snprintf(encoded + offset, sizeof(encoded) - offset, "\"}");

    // 写入 JSON 结尾前检查容量，避免静默截断。
    if (written < 0 || (size_t)written >= sizeof(encoded) - offset)
    {
        return MESSAGE_JSON_ERR_JSON;
    }

    offset += (size_t)written;

    if (out_capacity <= offset)
    {
        return MESSAGE_JSON_ERR_TOO_LARGE;
    }

    memcpy(out_json, encoded, offset + 1);
    *out_len = offset;

    return MESSAGE_JSON_OK;
}
