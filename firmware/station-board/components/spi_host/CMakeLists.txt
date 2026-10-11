# SPI controller side of the inter-board link to the bridge board (docs/spi-protocol.md).
idf_component_register(SRCS "spi_host.c"
                       INCLUDE_DIRS "include")
