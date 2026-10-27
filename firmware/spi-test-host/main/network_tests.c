#include "network_tests.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "sdkconfig.h"
#include "gateway_link.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

static const char *TAG = "network_test";
static unsigned round_number;
static TaskHandle_t tcp_worker, udp_worker;
static EventGroupHandle_t echoes_done;
static bool resolve(const char *host, int type, struct sockaddr_in *out)
{
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = type}, *result = NULL;
    int err = getaddrinfo(host, NULL, &hints, &result);
    if (err || !result)
    {
        if (result)
        {
            freeaddrinfo(result);
        }
        return false;
    }
    memcpy(out, result->ai_addr, sizeof(*out));
    freeaddrinfo(result);
    return true;
}
static bool connect_bounded(int fd, const struct sockaddr_in *address)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        return false;
    }
    int ret = connect(fd, (const struct sockaddr *)address, sizeof(*address));
    if (ret < 0)
    {
        if (errno != EINPROGRESS)
        {
            return false;
        }
        fd_set writes;
        FD_ZERO(&writes);
        FD_SET(fd, &writes);
        struct timeval timeout = {.tv_sec = 5};
        if (select(fd + 1, NULL, &writes, NULL, &timeout) <= 0)
        {
            return false;
        }
        int error = 0;
        socklen_t size = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) || error)
        {
            return false;
        }
    }
    return fcntl(fd, F_SETFL, flags) == 0;
}
static bool echo_test(int type)
{
    struct sockaddr_in address;
    if (!resolve(CONFIG_GW_TEST_ECHO_HOST, type, &address))
    {
        return false;
    }
    address.sin_port = htons(CONFIG_GW_TEST_ECHO_PORT);
    int fd = socket(AF_INET, type, 0);
    if (fd < 0)
    {
        return false;
    }
    struct timeval timeout = {.tv_sec = 5};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    bool ok = connect_bounded(fd, &address);
    /* Full-size UDP datagram and a TCP payload spanning at least two segments. */
    char sent[1472], received[1472];
    memset(sent, 'G', sizeof(sent));
    snprintf(sent, sizeof(sent), "gateway test round %u", round_number);
    size_t offset = 0;
    while (ok && offset < sizeof(sent))
    {
        int n = send(fd, sent + offset, sizeof(sent) - offset, 0);
        if (n <= 0 || (type == SOCK_DGRAM && n != sizeof(sent)))
        {
            ok = false;
        }
        else
        {
            offset += n;
        }
    }
    offset = 0;
    while (ok && offset < sizeof(received))
    {
        int n = recv(fd, received + offset, sizeof(received) - offset, 0);
        if (n <= 0 || (type == SOCK_DGRAM && n != sizeof(received)))
        {
            ok = false;
        }
        else
        {
            offset += n;
        }
    }
    ok = ok && memcmp(sent, received, sizeof(sent)) == 0;
    close(fd);
    return ok;
}
static void echo_worker(void *arg)
{
    int type = (int)(intptr_t)arg;
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "%s echo (1472 bytes): %s", type == SOCK_STREAM ? "TCP" : "UDP",
                 echo_test(type) ? "PASS" : "FAIL");
        xEventGroupSetBits(echoes_done, type == SOCK_STREAM ? 1 : 2);
    }
}
static bool https_test(void)
{
    if (strncmp(CONFIG_GW_TEST_HTTPS_URL, "https://", 8))
    {
        return false;
    }
    esp_http_client_config_t config = {.url = CONFIG_GW_TEST_HTTPS_URL,
                                       .crt_bundle_attach = esp_crt_bundle_attach,
                                       .timeout_ms = 8000,
                                       .disable_auto_redirect = true,
                                       .method = HTTP_METHOD_HEAD};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        return false;
    }
    esp_err_t err = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return err == ESP_OK && code >= 200 && code < 400;
}
static void tests(void *arg)
{
    (void)arg;
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_GW_TEST_NTP_SERVER);
    sntp.start = false;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp));
    bool sntp_started = false;
    for (;;)
    {
        if (!gateway_link_data_enabled())
        {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        ++round_number;
        struct sockaddr_in address;
        ESP_LOGI(TAG, "Round %u DNS: %s", round_number,
                 resolve(CONFIG_GW_TEST_DNS_NAME, SOCK_STREAM, &address) ? "PASS" : "FAIL");
        if (!sntp_started)
        {
            ESP_ERROR_CHECK(esp_netif_sntp_start());
            sntp_started = true;
        }
        bool clock_ok = time(NULL) >= 1700000000;
        if (!clock_ok)
        {
            clock_ok = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) == ESP_OK;
        }
        if (strlen(CONFIG_GW_TEST_ECHO_HOST))
        {
            xEventGroupClearBits(echoes_done, 3);
            xTaskNotifyGive(tcp_worker);
            xTaskNotifyGive(udp_worker);
        }
        if (clock_ok)
        {
            ESP_LOGI(TAG, "HTTPS with certificate verification: %s",
                     https_test() ? "PASS" : "FAIL");
        }
        else
        {
            ESP_LOGW(TAG,
                     "HTTPS: SKIP (clock not synchronized; certificate checks remain enabled)");
        }
        if (strlen(CONFIG_GW_TEST_ECHO_HOST))
        {
            xEventGroupWaitBits(echoes_done, 3, pdTRUE, pdTRUE, portMAX_DELAY);
        }
        else
        {
            ESP_LOGI(TAG, "TCP/UDP echo: SKIP (configure local echo host)");
        }
        ESP_LOGI(TAG, "Heap free=%u minimum=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
        vTaskDelay(pdMS_TO_TICKS(CONFIG_GW_TEST_INTERVAL_S * 1000));
    }
}
void network_tests_start(void)
{
    if (strlen(CONFIG_GW_TEST_ECHO_HOST))
    {
        echoes_done = xEventGroupCreate();
        configASSERT(echoes_done);
        configASSERT(xTaskCreate(echo_worker, "tcp_echo", 8192, (void *)(intptr_t)SOCK_STREAM, 4,
                                 &tcp_worker) == pdPASS);
        configASSERT(xTaskCreate(echo_worker, "udp_echo", 8192, (void *)(intptr_t)SOCK_DGRAM, 4,
                                 &udp_worker) == pdPASS);
    }
    configASSERT(xTaskCreate(tests, "network_tests", 8192, NULL, 4, NULL) == pdPASS);
}
