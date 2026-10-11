// HTTP client that talks to the one fixed target address only (R3).
#ifndef TARGET_CLIENT_H
#define TARGET_CLIENT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the target_client component.
 *
 * Stub: returns ESP_ERR_NOT_SUPPORTED until implemented.
 */
esp_err_t target_client_init(void);

#ifdef __cplusplus
}
#endif

#endif // TARGET_CLIENT_H
