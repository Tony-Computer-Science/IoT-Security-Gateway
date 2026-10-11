// Wi-Fi station connection and HTTPS server (port 443); serves web assets from LittleFS.
#ifndef NET_STATION_H
#define NET_STATION_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the net_station component.
 *
 * Stub: returns ESP_ERR_NOT_SUPPORTED until implemented.
 */
esp_err_t net_station_init(void);

#ifdef __cplusplus
}
#endif

#endif // NET_STATION_H
