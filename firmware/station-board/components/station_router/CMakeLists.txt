idf_component_register(SRCS "station_router.c" INCLUDE_DIRS "include"
    REQUIRES gateway_link access_point lwip esp_timer)
