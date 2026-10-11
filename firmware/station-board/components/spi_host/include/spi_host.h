// SPI controller side of the inter-board link to the bridge board (docs/spi-protocol.md).
#ifndef SPI_HOST_H
#define SPI_HOST_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the spi_host component.
 *
 * Stub: returns ESP_ERR_NOT_SUPPORTED until implemented.
 */
esp_err_t spi_host_init(void);

#ifdef __cplusplus
}
#endif

#endif // SPI_HOST_H
