#pragma once
#include "gateway_protocol.h"
#include "esp_netif.h"

/** Start once after NVS, ESP-NETIF, event loop and gateway_link initialization.
 * Missing credentials return ESP_ERR_INVALID_ARG and publish CONFIG_ERROR;
 * no access point or insecure fallback is started.
 */
esp_err_t uplink_wifi_start(void);
/** Borrowed handle; NULL when configuration validation prevented startup. */
esp_netif_t *uplink_wifi_netif(void);
/** Thread-safe snapshot; IP addresses remain network-order octets. */
void uplink_wifi_status(gw_status_t *status);
/** IPv4 lease acquired; does not probe internet connectivity. */
bool uplink_wifi_ready(void);
