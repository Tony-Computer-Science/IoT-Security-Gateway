idf_component_register(SRCS "access_point.c" INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif gateway_link lwip)
