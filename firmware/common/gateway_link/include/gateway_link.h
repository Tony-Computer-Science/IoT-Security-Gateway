#pragma once

/** One board-link/Ethernet endpoint per application (SPI or UART).
 * Shared state access is synchronized. Call from tasks, never from an ISR.
 * Only one supervisor may change the netif lifecycle; the transport task owns
 * transactions and session rotation. No caller owns returned netif storage.
 */
#include "esp_err.h"
#include "esp_netif.h"
#include "gateway_protocol.h"

/** Initialize the configured endpoint once; startup must stop on failure. */
esp_err_t gateway_link_init(void);
esp_netif_t *gateway_link_netif(void);
/** Immutable configured addressing; esp_netif IPv4 integers are network order. */
esp_netif_ip_info_t gateway_link_addresses(void);
uint32_t gateway_link_peer_ip(void);

/** Handshake complete and no pending reset. Does not imply upstream readiness. */
bool gateway_link_ready(void);
/** Data gate open, handshake complete and no pending reset. */
bool gateway_link_data_enabled(void);

/** Close immediately; transport rotates IDs at its next maintenance check.
 * A queued DMA transaction retains its old pairing and cannot reopen the gate.
 * Safe from the Wi-Fi event task. Does not reclaim RX buffers owned by lwIP.
 */
void gateway_link_request_reset(void);
/** Stop data immediately and invalidate queued generations; control still works. */
void gateway_link_close_gate(void);

/** Supervisor only. Bringing the netif up does NOT open the data gate.
 * On B: set netif up -> enable NAT -> open gate.
 * On teardown: close gate -> disable NAT -> set netif down.
 */
void gateway_link_set_netif_up(bool up);
/** Call only after route/NAT setup. Returns false if readiness changed meanwhile. */
bool gateway_link_open_gate(void);

/** Copy a diagnostic snapshot for periodic outbound STATUS messages. */
void gateway_link_publish_status(const gw_status_t *status);
/** Copy the latest peer status; false means no valid status in this pairing. */
bool gateway_link_get_remote_status(gw_status_t *status);
