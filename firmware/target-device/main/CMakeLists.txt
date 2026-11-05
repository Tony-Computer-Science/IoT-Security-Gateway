# Board C application, wire parser and addressable RGB LED driver.
idf_component_register(SRCS "main.c" "target_protocol.c" "target_led.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_client esp_timer
             esp_driver_rmt esp_driver_gpio esp_hw_support cjson)
