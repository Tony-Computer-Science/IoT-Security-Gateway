# ESP-IDF 6.1 project: Board C, Wi-Fi client and outbound UI polling.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(target_device)
if(NOT IDF_TARGET STREQUAL "esp32c6" AND NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "Board C firmware supports ESP32-C6 and ESP32-S3")
endif()
