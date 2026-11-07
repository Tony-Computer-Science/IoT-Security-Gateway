#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TARGET_COMMAND_ID_SIZE 33
#define TARGET_REPORT_SIZE 256
#define TARGET_RESPONSE_MAX 512

typedef struct
{
    char id[TARGET_COMMAND_ID_SIZE];
    bool on;
} target_command_t;

typedef enum
{
    TARGET_RESPONSE_INVALID,
    TARGET_RESPONSE_IDLE,
    TARGET_RESPONSE_COMMAND
} target_response_t;

bool target_device_id_valid(const char *id);
/* IDs are restricted ASCII, so no credentials or arbitrary strings enter JSON. */
bool target_report(char output[TARGET_REPORT_SIZE], const char *device_id, const char *boot_id,
                   bool on, const char *ack_id, bool ack_ok, uint32_t uptime_s);
target_response_t target_parse_response(const char *body, size_t length, target_command_t *out);
