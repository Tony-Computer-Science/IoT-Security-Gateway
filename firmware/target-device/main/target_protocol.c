#include "target_protocol.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

static bool hex_id(const char *id, size_t length)
{
    if (!id || strlen(id) != length)
    {
        return false;
    }
    for (size_t i = 0; i < length; ++i)
    {
        if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f')))
        {
            return false;
        }
    }
    return true;
}

bool target_device_id_valid(const char *id)
{
    if (!id || !*id || strlen(id) > 32)
    {
        return false;
    }
    for (; *id; ++id)
    {
        if (!((*id >= 'a' && *id <= 'z') || (*id >= 'A' && *id <= 'Z') ||
              (*id >= '0' && *id <= '9') || *id == '-' || *id == '_'))
        {
            return false;
        }
    }
    return true;
}

bool target_report(char output[TARGET_REPORT_SIZE], const char *device_id, const char *boot_id,
                   bool on, const char *ack_id, bool ack_ok, uint32_t uptime_s)
{
    if (!output || !target_device_id_valid(device_id) || !hex_id(boot_id, 16) ||
        (ack_id && !hex_id(ack_id, 32)))
    {
        return false;
    }
    char ack[36] = "null";
    if (ack_id)
    {
        snprintf(ack, sizeof(ack), "\"%s\"", ack_id);
    }
    int count =
        snprintf(output, TARGET_REPORT_SIZE,
                 "{\"device_id\":\"%s\",\"boot_id\":\"%s\",\"output\":\"%s\","
                 "\"ack_id\":%s,\"ack_ok\":%s,\"uptime_s\":%" PRIu32 "}",
                 device_id, boot_id, on ? "on" : "off", ack, ack_ok ? "true" : "false", uptime_s);
    return count > 0 && count < TARGET_REPORT_SIZE;
}

target_response_t target_parse_response(const char *body, size_t length, target_command_t *out)
{
    if (!body || !out || !length || length > TARGET_RESPONSE_MAX || memchr(body, 0, length))
    {
        return TARGET_RESPONSE_INVALID;
    }
    /* The wire format contains ASCII names/hex IDs only: no escapes are needed.
     * Bound nesting before cJSON allocates or recurses on untrusted responses. */
    unsigned depth = 0;
    bool quoted = false;
    for (size_t i = 0; i < length; ++i)
    {
        if (body[i] == '\\')
        {
            return TARGET_RESPONSE_INVALID;
        }
        if (body[i] == '"')
        {
            quoted = !quoted;
        }
        else if (!quoted && (body[i] == '{' || body[i] == '['))
        {
            if (++depth > 2)
            {
                return TARGET_RESPONSE_INVALID;
            }
        }
        else if (!quoted && (body[i] == '}' || body[i] == ']'))
        {
            if (!depth)
            {
                return TARGET_RESPONSE_INVALID;
            }
            --depth;
        }
    }
    if (depth || quoted)
    {
        return TARGET_RESPONSE_INVALID;
    }
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(body, length, &end, false);
    if (!root)
    {
        return TARGET_RESPONSE_INVALID;
    }
    while (end < body + length && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t'))
    {
        ++end;
    }
    target_response_t result = TARGET_RESPONSE_INVALID;
    cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "command");
    if (end == body + length && cJSON_IsObject(root) && cJSON_GetArraySize(root) == 1)
    {
        if (cJSON_IsNull(command))
        {
            result = TARGET_RESPONSE_IDLE;
        }
        else if (cJSON_IsObject(command) && cJSON_GetArraySize(command) == 2)
        {
            cJSON *id = cJSON_GetObjectItemCaseSensitive(command, "id");
            cJSON *action = cJSON_GetObjectItemCaseSensitive(command, "action");
            if (cJSON_IsString(id) && hex_id(id->valuestring, 32) && cJSON_IsString(action) &&
                (!strcmp(action->valuestring, "led_on") || !strcmp(action->valuestring, "led_off")))
            {
                memcpy(out->id, id->valuestring, TARGET_COMMAND_ID_SIZE);
                out->on = !strcmp(action->valuestring, "led_on");
                result = TARGET_RESPONSE_COMMAND;
            }
        }
    }
    cJSON_Delete(root);
    return result;
}
