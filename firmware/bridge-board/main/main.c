// BOARD B (bridge board): SPI peripheral, private AP, fixed-target HTTP client
//
// Scaffold only: boots and logs a banner. No features are started here yet.

#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "bridge_board";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " IoT Security Gateway: BOARD B (bridge board)");
    ESP_LOGI(TAG, " SPI peripheral, private AP, fixed-target HTTP client");
    ESP_LOGI(TAG, " ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, "========================================");
}
