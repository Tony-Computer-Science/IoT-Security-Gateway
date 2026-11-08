#include "gateway_diagnostics.h"
#include "sdkconfig.h"
#ifdef CONFIG_GW_LINK_DIAGNOSTICS
#include <string.h>
#include "gateway_link.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/netdb.h"

static void probe(void *argument)
{
    bool station = (bool)(intptr_t)argument;
    while (!gateway_link_data_enabled())
    {
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (station)
    {
        gw_status_t status;
        if (gateway_link_get_remote_status(&status))
        {
            esp_netif_dns_info_t dns = {.ip.type = ESP_IPADDR_TYPE_V4};
            memcpy(&dns.ip.u_addr.ip4.addr, status.dns[0], 4);
            if (dns.ip.u_addr.ip4.addr)
            {
                esp_netif_set_dns_info(gateway_link_netif(), ESP_NETIF_DNS_MAIN, &dns);
            }
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_netif_dns_info_t dns;
    if (esp_netif_get_dns_info(gateway_link_netif(), ESP_NETIF_DNS_MAIN, &dns) == ESP_OK)
    {
        ESP_LOGI("link_probe", "Resolver: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
    }
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *result = NULL;
    int resolved = getaddrinfo("example.com", NULL, &hints, &result);
    ESP_LOGI("link_probe", "%s DNS lookup: %s (code=%d)",
             station ? "Station over UART" : "Bridge upstream", resolved == 0 ? "PASS" : "FAIL",
             resolved);
    if (result)
    {
        freeaddrinfo(result);
    }
    esp_http_client_config_t config = {.url = "http://example.com/",
                                       .method = HTTP_METHOD_HEAD,
                                       .timeout_ms = 15000,
                                       .disable_auto_redirect = true};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client)
    {
        esp_err_t error = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI("link_probe", "%s HTTP: %s status=%d error=%s",
                 station ? "Station over UART" : "Bridge upstream",
                 error == ESP_OK && status >= 200 && status < 400 ? "PASS" : "FAIL", status,
                 esp_err_to_name(error));
        esp_http_client_cleanup(client);
    }
    vTaskDelete(NULL);
}
#endif

void gateway_diagnostics_start(bool station)
{
#ifdef CONFIG_GW_LINK_DIAGNOSTICS
    if (xTaskCreate(probe, "link_probe", 6144, (void *)(intptr_t)station, 4, NULL) != pdPASS)
    {
        ESP_LOGW("link_probe", "Unable to start diagnostic task");
    }
#else
    (void)station;
#endif
}
