idf_component_register(SRCS "gateway_link.c" INCLUDE_DIRS "include"
    REQUIRES gateway_protocol esp_netif esp_driver_spi esp_driver_gpio esp_driver_uart esp_timer esp_hw_support)
