// Command allow-list, input validation, and rate limiting (R3, R4).
#ifndef POLICY_H
#define POLICY_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the policy component.
 *
 * Stub: returns ESP_ERR_NOT_SUPPORTED until implemented.
 */
esp_err_t policy_init(void);

#ifdef __cplusplus
}
#endif

#endif // POLICY_H
