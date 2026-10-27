/* Pure Board A address/readiness and pre-NAT policy; covered by host tests. */
#include "gateway_access.h"
#include "gateway_bytes.h"

static bool unicast_address(uint32_t address)
{
    uint8_t first = address >> 24;
    return first != 0 && first != 127 && first < 224;
}

static bool in_subnet(uint32_t address, uint32_t local, uint32_t mask)
{
    return (address & mask) == (local & mask);
}

static bool client_address(uint32_t address, const gw_access_network_t *network)
{
    uint8_t host = address & 255;
    return in_subnet(address, network->ap_ip, GW_AP_MASK) && host > 1 && host < 255;
}

bool gw_access_config_valid(const gw_access_network_t *network)
{
    if (!network)
    {
        return false;
    }
    uint32_t ip = network->ap_ip;
    bool private = (ip & 0xff000000u) == 0x0a000000u || (ip & 0xfff00000u) == 0xac100000u ||
                   (ip & 0xffff0000u) == 0xc0a80000u;
    return private && (ip & 255) == 1 &&
           gw_link_addresses_valid(network->spi_ip, network->spi_peer, network->spi_mask) &&
           !gw_subnets_overlap(ip, GW_AP_MASK, network->spi_ip, network->spi_mask);
}

gw_access_state_t gw_access_bridge_state(const gw_status_t *bridge,
                                         const gw_access_network_t *network)
{
    if (!bridge || !gw_access_config_valid(network) ||
        (bridge->flags & (GW_IPV4 | GW_FORWARDING)) != (GW_IPV4 | GW_FORWARDING) ||
        (bridge->flags & (GW_CONFIG_ERROR | GW_OVERLAP)))
    {
        return GW_ACCESS_OFFLINE;
    }
    if (read_u32_be(bridge->link_local) != network->spi_peer ||
        read_u32_be(bridge->link_peer) != network->spi_ip ||
        read_u32_be(bridge->link_mask) != network->spi_mask)
    {
        return GW_ACCESS_ADDRESS_MISMATCH;
    }
    uint32_t upstream_ip = read_u32_be(bridge->ip);
    uint32_t upstream_mask = read_u32_be(bridge->mask);
    uint32_t host_bits = ~upstream_mask;
    if (!unicast_address(upstream_ip) || !upstream_mask || (host_bits & (host_bits + 1)))
    {
        return GW_ACCESS_OFFLINE;
    }
    if (gw_subnets_overlap(network->ap_ip, GW_AP_MASK, upstream_ip, upstream_mask) ||
        gw_subnets_overlap(network->spi_ip, network->spi_mask, upstream_ip, upstream_mask))
    {
        return GW_ACCESS_SUBNET_OVERLAP;
    }
    return GW_ACCESS_READY;
}

void gw_access_dns(const gw_status_t *bridge, const gw_access_network_t *network,
                   uint32_t servers[2])
{
    servers[0] = servers[1] = 0;
    unsigned count = 0;
    for (unsigned i = 0; i < 2; ++i)
    {
        uint32_t ip = read_u32_be(bridge->dns[i]);
        if (unicast_address(ip) && !in_subnet(ip, network->ap_ip, GW_AP_MASK) &&
            !in_subnet(ip, network->spi_ip, network->spi_mask) && ip != servers[0])
        {
            servers[count++] = ip;
        }
    }
}

bool gw_access_ip4_allowed(const uint8_t *packet, size_t contiguous_length, size_t total_length,
                           const gw_access_network_t *network, bool from_clients,
                           bool forwarding_ready)
{
    if (!network || !gw_ip4_packet_valid(packet, contiguous_length, total_length))
    {
        return false;
    }
    uint32_t source = read_u32_be(packet + 12);
    uint32_t destination = read_u32_be(packet + 16);
    const uint8_t *transport = packet + (packet[0] & 15) * 4;
    if (!from_clients)
    {
        /* Replies enter via A's SPI address before NAT restores the client.
         * A direct SPI/upstream route to the client subnet cannot bypass NAT. */
        return forwarding_ready && destination == network->spi_ip;
    }

    bool local_or_broadcast = destination == network->ap_ip || destination == UINT32_MAX ||
                              destination == (network->ap_ip | ~GW_AP_MASK);
    if (packet[9] == 17 && read_u16_be(transport) == 68 && read_u16_be(transport + 2) == 67 &&
        local_or_broadcast && (source == 0 || client_address(source, network)))
    {
        return true; /* DHCP discover/request must work before a client has an IP. */
    }
    if (!client_address(source, network))
    {
        return false;
    }
    if (destination == network->ap_ip)
    {
        return packet[9] == 1 && transport[0] == 8 && transport[1] == 0; /* gateway ping */
    }
    return forwarding_ready && !in_subnet(destination, network->ap_ip, GW_AP_MASK) &&
           !in_subnet(destination, network->spi_ip, network->spi_mask);
}
