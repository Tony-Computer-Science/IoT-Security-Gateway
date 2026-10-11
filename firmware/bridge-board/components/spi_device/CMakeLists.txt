# SPI peripheral side of the inter-board link; validates every frame (R3, docs/spi-protocol.md).
idf_component_register(SRCS "spi_device.c"
                       INCLUDE_DIRS "include")
