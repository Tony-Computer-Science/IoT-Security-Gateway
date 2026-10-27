#pragma once
#include "gateway_protocol.h"

/** Board A policy uses host-order IPv4 integers throughout. The AP is a /24
 * with gateway .1 and DHCP leases .2-.100, within an RFC1918 private range. */
#define GW_AP_MASK UINT32_C(0xffffff00)

typedef enum
{
    GW_ACCESS_OFFLINE,
    GW_ACCESS_ADDRESS_MISMATCH,
    GW_ACCESS_SUBNET_OVERLAP,
    GW_ACCESS_READY,
} gw_access_state_t;

typedef struct
{
    uint32_t ap_ip;
    uint32_t spi_ip;
    uint32_t spi_peer;
    uint32_t spi_mask;
} gw_access_network_t;

bool gw_access_config_valid(const gw_access_network_t *network);
/** Validate B's current status, address agreement and all three subnets. */
gw_access_state_t gw_access_bridge_state(const gw_status_t *bridge,
                                         const gw_access_network_t *network);
/** Select usable advertised DNS servers, compacting a backup into the first slot.
 * Zero fills unused slots; output is host order. No hardcoded public fallback. */
void gw_access_dns(const gw_status_t *bridge, const gw_access_network_t *network,
                   uint32_t servers[2]);

/** Input filter before NAT. from_clients=true means the AP, false means SPI.
 * Local DHCP and gateway ping remain available when forwarding is disabled.
 * This is IP routing policy, not Wi-Fi client isolation or per-device identity. */
bool gw_access_ip4_allowed(const uint8_t *packet, size_t contiguous_length, size_t total_length,
                           const gw_access_network_t *network, bool from_clients,
                           bool forwarding_ready);
