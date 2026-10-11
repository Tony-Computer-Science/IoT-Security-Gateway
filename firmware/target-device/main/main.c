// BOARD C (target device): INTENTIONALLY VULNERABLE evaluation target - lab use only
//
// Scaffold only: boots and logs a banner. No features are started here yet.

#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "target_device";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " IoT Security Gateway: BOARD C (target device)");
    ESP_LOGI(TAG, " INTENTIONALLY VULNERABLE evaluation target - lab use only");
    ESP_LOGI(TAG, " ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, "========================================");
}
