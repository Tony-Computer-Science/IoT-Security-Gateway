// Login, session token issuance, expiry, and invalidation on restart (R2).
#ifndef AUTH_H
#define AUTH_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the auth component.
 *
 * Stub: returns ESP_ERR_NOT_SUPPORTED until implemented.
 */
esp_err_t auth_init(void);

#ifdef __cplusplus
}
#endif

#endif // AUTH_H
