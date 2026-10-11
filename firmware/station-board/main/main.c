// BOARD A (station board): HTTPS gateway, authentication, policy, event log, SPI controller
//
// Scaffold only: boots and logs a banner. No features are started here yet.

#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "station_board";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " IoT Security Gateway: BOARD A (station board)");
    ESP_LOGI(TAG, " HTTPS gateway, authentication, policy, event log, SPI controller");
    ESP_LOGI(TAG, " ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, "========================================");
}
