#pragma once
#include "esp_netif.h"
#include "gateway_access.h"

/** Start once after NVS, ESP-NETIF, event loop and gateway_link initialization.
 * Invalid credentials/addressing return ESP_ERR_INVALID_ARG; no open fallback.
 * DHCP remains available during upstream outages. No station radio is started. */
esp_err_t access_point_start(void);
esp_netif_t *access_point_netif(void);          /**< Borrowed handle, NULL before valid setup. */
bool access_point_running(void);                /**< Synchronized Wi-Fi AP lifecycle state. */
gw_access_network_t access_point_network(void); /**< Immutable host-order addresses. */

/** Supervisor only. Update DNS advertised by DHCP using network-order addresses.
 * Changed options restart DHCP and briefly deauthenticate existing clients so
 * their next DHCP exchange refreshes DNS. Unchanged values do nothing. A zero
 * pair disables the DNS option; it does not advertise a nonexistent resolver. */
esp_err_t access_point_set_dns(const uint32_t servers[2]);
