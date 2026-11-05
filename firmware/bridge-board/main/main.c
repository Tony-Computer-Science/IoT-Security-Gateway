#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "gateway_link.h"
#include "uplink_wifi.h"
#include "bridge_router.h"
#include "board_led.h"
#include "gateway_diagnostics.h"

#ifdef CONFIG_GW_SPI_CONTROLLER
#error "Board B must use the bridge endpoint; disable GW_SPI_CONTROLLER"
#endif
#if !CONFIG_LWIP_IP_FORWARD || !CONFIG_LWIP_IPV4_NAPT
#error "Board B requires IPv4 forwarding and NAPT"
#endif
#if CONFIG_LWIP_IPV6 || CONFIG_LWIP_IPV4_NAPT_PORTMAP
#error "Outbound-only gateway configuration excludes IPv6 and inbound port mapping"
#endif

void app_main(void)
{
    esp_err_t led_error = board_led_show_role(false);
    if (led_error != ESP_OK)
    {
        ESP_LOGW("bridge", "Identity LED unavailable: %s", esp_err_to_name(led_error));
    }
    /* Never erase provisioned NVS silently on initialization failure. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gateway_link_init());
    esp_err_t err = uplink_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE("bridge", "Uplink unavailable: %s; board link diagnostics remain available",
                 esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(bridge_router_start());
    gateway_diagnostics_start(false);
}
