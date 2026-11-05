#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t target_led_init(void);
esp_err_t target_led_set(bool on);
