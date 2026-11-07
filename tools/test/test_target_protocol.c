#include "target_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static target_response_t parse(const char *json, target_command_t *out)
{
    return target_parse_response(json, strlen(json), out);
}

int main(void)
{
    target_command_t command = {0};
    assert(parse("{\"command\":null}", &command) == TARGET_RESPONSE_IDLE);
    const char *on =
        "{\"command\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"action\":\"led_on\"}}";
    assert(parse(on, &command) == TARGET_RESPONSE_COMMAND && command.on);
    assert(!strcmp(command.id, "0123456789abcdef0123456789abcdef"));
    assert(
        parse(
            "{\"command\":{\"action\":\"led_off\",\"id\":\"0123456789abcdef0123456789abcdef\"}} \n",
            &command) == TARGET_RESPONSE_COMMAND &&
        !command.on);
    const char *invalid[] = {
        "",
        "null",
        "{}",
        "[]",
        "{\"command\":null}garbage",
        "{\"command\":null,\"extra\":0}",
        "{\"command\":null,\"command\":null}",
        "{\"command\":true}",
        "{\"command\":[]}",
        "{\"command\":{\"id\":\"short\",\"action\":\"led_on\"}}",
        "{\"command\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"action\":\"reboot\"}}",
        "{\"command\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"action\":[]}}",
        "{\"command\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"action\":\"led_on\",\"extra\":"
        "0}}",
        "{\"command\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"id\":"
        "\"0123456789abcdef0123456789abcdef\"}}",
        "{\"command\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"action\":\"led_"
        "on\\u0000extra\"}}",
        "{\"command\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"action\":\"led_\\u006fn\"}}",
        "{\"command\":{\"id\":{},\"action\":\"led_on\"}}",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        assert(parse(invalid[i], &command) == TARGET_RESPONSE_INVALID);
    }
    for (size_t length = 0; length < strlen(on); ++length)
    {
        assert(target_parse_response(on, length, &command) == TARGET_RESPONSE_INVALID);
    }
    char long_body[TARGET_RESPONSE_MAX + 2];
    memset(long_body, ' ', sizeof(long_body));
    assert(target_parse_response(long_body, sizeof(long_body), &command) ==
           TARGET_RESPONSE_INVALID);
    char nul[] = "{\"command\":null}\0extra";
    assert(target_parse_response(nul, sizeof(nul) - 1, &command) == TARGET_RESPONSE_INVALID);
    char report[TARGET_REPORT_SIZE];
    assert(target_report(report, "iot-c6", "0123456789abcdef", true, command.id, true, UINT32_MAX));
    assert(strstr(report, "\"output\":\"on\"") && strstr(report, "4294967295"));
    assert(target_report(report, "iot-c6", "0123456789abcdef", false, NULL, true, 0));
    assert(strstr(report, "\"ack_id\":null"));
    assert(!target_report(report, "bad\"id", "0123456789abcdef", false, NULL, true, 0));
    assert(!target_report(report, "iot-c6", "bad", false, NULL, true, 0));
    assert(!target_report(report, "iot-c6", "0123456789abcdef", false, "bad", true, 0));
    assert(!target_device_id_valid("") && !target_device_id_valid("contains space"));
    assert(target_device_id_valid("iot_C6-1"));
    puts("PASS Board C protocol: valid commands/reports, bounded JSON, truncation, duplicates and "
         "invalid actions");
}
