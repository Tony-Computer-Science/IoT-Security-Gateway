#pragma once
#include "esp_err.h"

/** Start the sole owner of Board A's SPI netif/NAPT lifecycle after AP setup.
 * AP configuration failure is allowed: SPI control remains available, while
 * client forwarding stays closed. Start once for application lifetime. */
esp_err_t station_router_start(void);
