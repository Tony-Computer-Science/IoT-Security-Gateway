# Wi-Fi station connection and HTTPS server (port 443); serves web assets from LittleFS.
idf_component_register(SRCS "net_station.c"
                       INCLUDE_DIRS "include")
