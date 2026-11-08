#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Call once at startup: blue for station, red for bridge. LED errors must
 * not stop networking. Does nothing when the RGB LED option is disabled. */
esp_err_t board_led_show_role(bool station);
