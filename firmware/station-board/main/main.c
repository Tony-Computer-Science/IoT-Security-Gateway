#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "gateway_link.h"
#include "access_point.h"
#include "station_router.h"

#ifndef CONFIG_GW_SPI_CONTROLLER
#error "Board A requires the SPI controller endpoint"
#endif
#if !CONFIG_LWIP_IP_FORWARD || !CONFIG_LWIP_IPV4_NAPT
#error "Board A requires IPv4 forwarding and NAPT"
#endif
#if CONFIG_LWIP_IPV6 || CONFIG_LWIP_IPV4_NAPT_PORTMAP
#error "IPv6 and inbound port mapping are outside this router configuration"
#endif

void app_main(void)
{
    /* Stop on NVS failure; never erase stored data silently. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gateway_link_init());
    ESP_ERROR_CHECK(esp_netif_set_default_netif(gateway_link_netif()));
    esp_err_t error = access_point_start();
    if (error != ESP_OK)
    {
        ESP_LOGE("station", "AP unavailable: %s; configure it in menuconfig",
                 esp_err_to_name(error));
    }
    ESP_ERROR_CHECK(station_router_start());
}
