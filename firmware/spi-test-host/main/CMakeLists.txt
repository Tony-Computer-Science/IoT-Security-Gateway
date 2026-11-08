idf_component_register(SRCS "main.c" "network_tests.c" INCLUDE_DIRS "."
    REQUIRES gateway_link esp_event esp_netif esp_http_client mbedtls lwip)
