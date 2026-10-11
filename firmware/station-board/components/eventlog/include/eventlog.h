// Bounded in-memory event log of auth, command, rate-limit, and target events (R5).
#ifndef EVENTLOG_H
#define EVENTLOG_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the eventlog component.
 *
 * Stub: returns ESP_ERR_NOT_SUPPORTED until implemented.
 */
esp_err_t eventlog_init(void);

#ifdef __cplusplus
}
#endif

#endif // EVENTLOG_H
