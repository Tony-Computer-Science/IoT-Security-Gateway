idf_component_register(SRCS "gateway_diagnostics.c" INCLUDE_DIRS "include"
    REQUIRES gateway_link esp_http_client esp_netif lwip)
