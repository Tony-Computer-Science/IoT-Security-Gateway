#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "gateway_link.h"
#include "uplink_wifi.h"
#include "bridge_router.h"

#ifdef CONFIG_GW_SPI_CONTROLLER
#error "Board B must use the SPI peripheral endpoint; disable GW_SPI_CONTROLLER"
#endif

void app_main(void)
{
    /* Never erase provisioned NVS silently on initialization failure. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gateway_link_init());
    esp_err_t err = uplink_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE("bridge", "Uplink unavailable: %s; SPI diagnostics remain available",
                 esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(bridge_router_start());
}
