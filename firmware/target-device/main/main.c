/* Board C: mutually authenticated TLS to its provisioned PC. All connections are
 * initiated toward the PC; there is no listening socket on this board. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include "sdkconfig.h"
#include "target_led.h"
#include "target_protocol.h"
#include "target_network_hooks.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

#define IPV4_READY BIT0
static const char *TAG = "iot_device";
static EventGroupHandle_t network_events;
extern const char target_ca[] asm("_binary_target_ca_start");
extern const char target_cert[] asm("_binary_target_cert_start");
extern const char target_key[] asm("_binary_target_key_start");

typedef struct
{
    char bytes[TARGET_RESPONSE_MAX + 1];
    size_t length;
    bool overflow;
} response_t;

static void network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        const ip_event_got_ip_t *event = data;
        xEventGroupSetBits(network_events, IPV4_READY);
        ESP_LOGI(TAG, "Joined station AP; IPv4 " IPSTR, IP2STR(&event->ip_info.ip));
    }
    else if ((base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) ||
             (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP))
    {
        xEventGroupClearBits(network_events, IPV4_READY);
        if (base == WIFI_EVENT)
        {
            const wifi_event_sta_disconnected_t *event = data;
            ESP_LOGW(TAG, "Wi-Fi unavailable, reason=%u; commands paused", event->reason);
        }
        else
        {
            ESP_LOGW(TAG, "IPv4 unavailable; commands paused");
        }
    }
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    response_t *response = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_HEADER &&
        !strcasecmp(event->header_key, "X-Device-Time") && strlen(event->header_value) <= 10)
    {
        /* Headers arrive only after the provisioned PC's TLS identity verified.
         * Bootstrap uses firmware build time; authenticated PC time refines it. */
        char *end;
        unsigned long long seconds = strtoull(event->header_value, &end, 10);
        if (end != event->header_value && !*end && seconds >= TARGET_BUILD_EPOCH &&
            seconds <= UINT32_MAX)
        {
            struct timeval now = {.tv_sec = (time_t)seconds};
            settimeofday(&now, NULL);
        }
    }
    else if (event->event_id == HTTP_EVENT_ON_DATA)
    {
        if (event->data_len < 0 || (size_t)event->data_len > TARGET_RESPONSE_MAX - response->length)
        {
            response->overflow = true;
            return ESP_FAIL;
        }
        memcpy(response->bytes + response->length, event->data, event->data_len);
        response->length += event->data_len;
    }
    return ESP_OK;
}

static void device_task(void *unused)
{
    (void)unused;
    char boot_id[17];
    snprintf(boot_id, sizeof(boot_id), "%08" PRIx32 "%08" PRIx32, esp_random(), esp_random());
    char acknowledged[TARGET_COMMAND_ID_SIZE] = {0};
    bool on = false, ack_ok = true;
    unsigned reconnect_ms = 1000, failures = 0;
    char url[96];
    snprintf(url, sizeof(url), "https://%s:%d/device/poll", CONFIG_TARGET_UI_HOST,
             CONFIG_TARGET_UI_PORT);
    response_t response = {0};
    esp_http_client_config_t config = {.url = url,
                                       .method = HTTP_METHOD_POST,
                                       .event_handler = http_event,
                                       .user_data = &response,
                                       .timeout_ms = 5000,
                                       .cert_pem = target_ca,
                                       .client_cert_pem = target_cert,
                                       .client_key_pem = target_key,
                                       .disable_auto_redirect = true,
                                       .buffer_size = 512,
                                       .buffer_size_tx = 512};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Cannot allocate HTTP client");
        vTaskDelete(NULL);
        return;
    }
    ESP_ERROR_CHECK(esp_http_client_set_header(client, "Content-Type", "application/json"));
    ESP_ERROR_CHECK(esp_http_client_set_header(client, "Accept", "application/json"));
    ESP_LOGI(TAG, "Polling UI computer at %s:%d; device=%s; LED GPIO%d", CONFIG_TARGET_UI_HOST,
             CONFIG_TARGET_UI_PORT, CONFIG_TARGET_DEVICE_ID, CONFIG_TARGET_LED_GPIO);
    for (;;)
    {
        if (!(xEventGroupGetBits(network_events) & IPV4_READY))
        {
            esp_err_t error = esp_wifi_connect();
            if (error != ESP_OK)
            {
                ESP_LOGW(TAG, "Wi-Fi connection attempt: %s", esp_err_to_name(error));
            }
            if (!(xEventGroupWaitBits(network_events, IPV4_READY, pdFALSE, pdFALSE,
                                      pdMS_TO_TICKS(15000)) &
                  IPV4_READY))
            {
                esp_wifi_disconnect();
                vTaskDelay(pdMS_TO_TICKS(reconnect_ms));
                reconnect_ms = reconnect_ms < 15000 ? reconnect_ms * 2 : 30000;
                continue;
            }
            reconnect_ms = 1000;
        }
        char report[TARGET_REPORT_SIZE];
        if (!target_report(report, CONFIG_TARGET_DEVICE_ID, boot_id, on,
                           acknowledged[0] ? acknowledged : NULL, ack_ok,
                           esp_timer_get_time() / 1000000))
        {
            ESP_LOGE(TAG, "Cannot encode device report; commands paused");
            esp_http_client_cleanup(client);
            vTaskDelete(NULL);
            return;
        }
        memset(&response, 0, sizeof(response));
        ESP_ERROR_CHECK(esp_http_client_set_post_field(client, report, strlen(report)));
        esp_err_t error = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        response.bytes[response.length] = 0;
        target_command_t command;
        target_response_t parsed = TARGET_RESPONSE_INVALID;
        if (error == ESP_OK && status == 200 && !response.overflow &&
            esp_http_client_is_complete_data_received(client) &&
            (xEventGroupGetBits(network_events) & IPV4_READY))
        {
            parsed = target_parse_response(response.bytes, response.length, &command);
        }
        if (parsed == TARGET_RESPONSE_INVALID)
        {
            if (failures++ % 10 == 0)
            {
                ESP_LOGW(TAG, "UI poll unavailable/invalid: HTTP %d, %s; LED state retained",
                         status, esp_err_to_name(error));
            }
            esp_http_client_close(client);
        }
        else
        {
            if (failures)
            {
                ESP_LOGI(TAG, "UI communication restored");
            }
            failures = 0;
            if (parsed == TARGET_RESPONSE_COMMAND && strcmp(command.id, acknowledged))
            {
                error = target_led_set(command.on);
                ack_ok = error == ESP_OK;
                if (ack_ok)
                {
                    on = command.on;
                }
                memcpy(acknowledged, command.id, sizeof(acknowledged));
                ESP_LOGI(TAG, "LED %s; driver result=%s", on ? "on (green)" : "off",
                         esp_err_to_name(error));
            }
        }
        unsigned delay_ms =
            failures ? (failures < 5 ? 1000 * failures : 5000) : CONFIG_TARGET_POLL_MS;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(target_network_init() ? ESP_OK : ESP_ERR_INVALID_ARG);
    if (time(NULL) < (time_t)TARGET_BUILD_EPOCH)
    {
        struct timeval build_time = {.tv_sec = (time_t)TARGET_BUILD_EPOCH};
        ESP_ERROR_CHECK(settimeofday(&build_time, NULL) == 0 ? ESP_OK : ESP_FAIL);
    }
    ESP_ERROR_CHECK(target_led_init());
    esp_ip4_addr_t ui_ip;
    if (!strlen(CONFIG_TARGET_WIFI_SSID) || strlen(CONFIG_TARGET_WIFI_SSID) > 32 ||
        strlen(CONFIG_TARGET_WIFI_PASSWORD) < 8 || strlen(CONFIG_TARGET_WIFI_PASSWORD) > 63 ||
        !target_device_id_valid(CONFIG_TARGET_DEVICE_ID) ||
        esp_netif_str_to_ip4(CONFIG_TARGET_UI_HOST, &ui_ip) != ESP_OK ||
        (ntohl(ui_ip.addr) >> 24) == 0 || (ntohl(ui_ip.addr) >> 24) == 127 ||
        (ntohl(ui_ip.addr) >> 24) >= 224)
    {
        ESP_LOGE(TAG, "Configure Board C Wi-Fi, UI IPv4 address and device ID in menuconfig");
        return;
    }
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    network_events = xEventGroupCreate();
    if (!network_events || !esp_netif_create_default_wifi_sta())
    {
        ESP_LOGE(TAG, "Cannot allocate network state");
        return;
    }
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, network_event, NULL));
    wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initialization));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi = {0};
    memcpy(wifi.sta.ssid, CONFIG_TARGET_WIFI_SSID, strlen(CONFIG_TARGET_WIFI_SSID));
    memcpy(wifi.sta.password, CONFIG_TARGET_WIFI_PASSWORD, strlen(CONFIG_TARGET_WIFI_PASSWORD));
    wifi.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    if (xTaskCreate(device_task, "iot_poll", 8192, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Cannot start command polling");
    }
}
