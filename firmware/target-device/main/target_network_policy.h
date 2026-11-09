#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* pc_ipv4 is host order. Non-PC exceptions: DHCP and ping on the AP /24. */
bool target_ipv4_allowed(const uint8_t *packet, size_t length, uint32_t pc_ipv4);
