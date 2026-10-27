#pragma once

/** Start the repeatable DNS, verified HTTPS and optional concurrent echo tests.
 * Call once after gateway_link_init(). Workers wait for the SPI data gate;
 * they do not initialize Wi-Fi or bypass TLS verification when offline.
 */
void network_tests_start(void);
