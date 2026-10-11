// SPI peripheral side of the inter-board link; validates every frame (R3, docs/spi-protocol.md).
#ifndef SPI_DEVICE_H
#define SPI_DEVICE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the spi_device component.
 *
 * Stub: returns ESP_ERR_NOT_SUPPORTED until implemented.
 */
esp_err_t spi_device_init(void);

#ifdef __cplusplus
}
#endif

#endif // SPI_DEVICE_H
