# Board C application, wire parser and addressable RGB LED driver.
idf_component_register(SRCS "main.c" "target_protocol.c" "target_led.c"
                           "target_network.c" "target_network_policy.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_client esp_timer
             esp_driver_rmt esp_driver_gpio esp_hw_support cjson lwip)

set(TARGET_IDENTITY_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../local-backups/device-identity"
    CACHE PATH "Private PC/target TLS identity directory")
foreach(identity ca.pem device.pem device.key)
    if(NOT EXISTS "${TARGET_IDENTITY_DIR}/${identity}")
        message(FATAL_ERROR "Provision private TLS identities with tools/provision_device_tls.py")
    endif()
endforeach()
target_add_binary_data(${COMPONENT_LIB} "${TARGET_IDENTITY_DIR}/ca.pem" TEXT RENAME_TO target_ca)
target_add_binary_data(${COMPONENT_LIB} "${TARGET_IDENTITY_DIR}/device.pem" TEXT RENAME_TO target_cert)
target_add_binary_data(${COMPONENT_LIB} "${TARGET_IDENTITY_DIR}/device.key" TEXT RENAME_TO target_key)
string(TIMESTAMP TARGET_BUILD_EPOCH "%s" UTC)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TARGET_BUILD_EPOCH=${TARGET_BUILD_EPOCH})
