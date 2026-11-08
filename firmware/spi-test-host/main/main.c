#include <string.h>
#include "gateway_link.h"
#include "network_tests.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef CONFIG_GW_SPI_CONTROLLER
#error "The Board A test application requires GW_SPI_CONTROLLER"
#endif

/* B's advertised forwarding/address state is the test host's only source of
 * readiness. Pairing alone is insufficient: B may be offline or misconfigured. */
static void supervisor(void *arg)
{
    (void)arg;
    bool active = false;
    for (;;)
    {
        gw_status_t status;
        esp_netif_ip_info_t local = gateway_link_addresses();
        uint32_t peer = gateway_link_peer_ip();
        bool valid = gateway_link_get_remote_status(&status) && (status.flags & GW_FORWARDING) &&
                     !memcmp(status.link_local, &peer, 4) &&
                     !memcmp(status.link_peer, &local.ip.addr, 4) &&
                     !memcmp(status.link_mask, &local.netmask.addr, 4);
        if (active && (!valid || !gateway_link_data_enabled()))
        {
            gateway_link_set_netif_up(false);
            active = false;
            ESP_LOGW("test_host", "Relay unavailable; data interface closed");
        }
        if (valid)
        {
            for (int i = 0; i < 2; ++i)
            {
                esp_netif_dns_info_t dns = {.ip.type = ESP_IPADDR_TYPE_V4};
                memcpy(&dns.ip.u_addr.ip4.addr, status.dns[i], 4);
                ESP_ERROR_CHECK(esp_netif_set_dns_info(
                    gateway_link_netif(), i ? ESP_NETIF_DNS_BACKUP : ESP_NETIF_DNS_MAIN, &dns));
            }
            if (!active)
            {
                gateway_link_set_netif_up(true);
                active = gateway_link_open_gate();
                if (active)
                {
                    ESP_LOGI("test_host",
                             "Relay ready; tests use SPI only (Wi-Fi not initialized)");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gateway_link_init());
    ESP_ERROR_CHECK(esp_netif_set_default_netif(gateway_link_netif()));
    configASSERT(xTaskCreate(supervisor, "host_supervisor", 4096, NULL, 6, NULL) == pdPASS);
    network_tests_start();
}
