/* Pure address and pre-NAT packet policy. Keep SDK/lwIP calls in bridge_router. */
#include "gateway_protocol.h"
#include "gateway_bytes.h"

enum
{
    IPV4_MIN_HEADER = 20,
    TCP_MIN_HEADER = 20,
    UDP_HEADER = 8,
    ICMP_MIN_HEADER = 8,
    IP_PROTOCOL_ICMP = 1,
    IP_PROTOCOL_TCP = 6,
    IP_PROTOCOL_UDP = 17,
    IPV4_FRAGMENT_MASK = 0x3fff,
};

bool gw_subnets_overlap(uint32_t address_a, uint32_t mask_a, uint32_t address_b, uint32_t mask_b)
{
    uint32_t shared_mask = mask_a & mask_b;
    return (address_a & shared_mask) == (address_b & shared_mask);
}

bool gw_can_forward(bool peer_ready, bool ipv4_ready, bool overlap, bool resetting)
{
    return peer_ready && ipv4_ready && !overlap && !resetting;
}

bool gw_link_addresses_valid(uint32_t local, uint32_t peer, uint32_t mask)
{
    const uint32_t point_to_point_mask = 0xfffffffcu;
    const uint32_t host_bits = 3;
    bool usable_local = (local & host_bits) != 0 && (local & host_bits) != host_bits;
    bool usable_peer = (peer & host_bits) != 0 && (peer & host_bits) != host_bits;
    return mask == point_to_point_mask && local != peer && (local & mask) == (peer & mask) &&
           usable_local && usable_peer;
}

/* NAPT parses these fields before the SDK's normal transport input validation. */
static bool transport_header_valid(const uint8_t *packet, size_t contiguous_length,
                                   size_t ip_header_length, size_t ip_length)
{
    uint8_t protocol = packet[9];
    if (protocol != IP_PROTOCOL_TCP && protocol != IP_PROTOCOL_UDP && protocol != IP_PROTOCOL_ICMP)
    {
        return false;
    }

    size_t minimum = protocol == IP_PROTOCOL_TCP ? TCP_MIN_HEADER : ICMP_MIN_HEADER;
    if (ip_length < ip_header_length + minimum || contiguous_length < ip_header_length + minimum)
    {
        return false;
    }

    const uint8_t *transport = packet + ip_header_length;
    size_t transport_length = ip_length - ip_header_length;
    if (protocol == IP_PROTOCOL_TCP)
    {
        size_t tcp_header_length = (transport[12] >> 4) * 4;
        return tcp_header_length >= TCP_MIN_HEADER && tcp_header_length <= transport_length;
    }
    if (protocol == IP_PROTOCOL_UDP)
    {
        size_t udp_length = read_u16_be(transport + 4);
        return udp_length >= UDP_HEADER && udp_length <= transport_length;
    }
    return true;
}

static bool ip_header_valid(const uint8_t *packet, size_t contiguous_length, size_t total_length)
{
    if (!packet || contiguous_length < IPV4_MIN_HEADER || packet[0] >> 4 != 4)
    {
        return false;
    }

    size_t ip_header_length = (packet[0] & 15) * 4;
    size_t ip_length = read_u16_be(packet + 2);
    if (ip_header_length < IPV4_MIN_HEADER || ip_header_length > contiguous_length ||
        ip_length < ip_header_length || ip_length > total_length)
    {
        return false;
    }

    return true;
}

bool gw_ip4_packet_valid(const uint8_t *packet, size_t contiguous_length, size_t total_length)
{
    if (!ip_header_valid(packet, contiguous_length, total_length))
    {
        return false;
    }
    /* The SDK invokes NAPT before reassembly. A fragment is not an L4 header. */
    return !(read_u16_be(packet + 6) & IPV4_FRAGMENT_MASK) &&
           transport_header_valid(packet, contiguous_length, (packet[0] & 15) * 4,
                                  read_u16_be(packet + 2));
}

bool gw_ip4_allowed(const uint8_t *packet, size_t contiguous_length, size_t total_length,
                    const gw_ipv4_policy_t *policy)
{
    if (!policy || !ip_header_valid(packet, contiguous_length, total_length) ||
        (policy->ingress != GW_INGRESS_LOCAL &&
         !gw_ip4_packet_valid(packet, contiguous_length, total_length)))
    {
        return false;
    }

    if (policy->ingress == GW_INGRESS_SPI)
    {
        if (!policy->forwarding_ready || read_u32_be(packet + 12) != policy->peer_ipv4)
        {
            return false;
        }
    }
    if (policy->ingress == GW_INGRESS_UPLINK)
    {
        /* Outside hosts cannot initiate TCP connections, even if their
         * chosen tuple happens to coincide with an existing NAT mapping. */
        size_t header_length = (packet[0] & 15) * 4;
        if (packet[9] == IP_PROTOCOL_TCP && (packet[header_length + 13] & 0x12) == 0x02)
        {
            return false;
        }
        /* A manually installed upstream route must not bypass NAT. */
        uint32_t destination_network = read_u32_be(packet + 16) & policy->link_mask;
        if (destination_network == (policy->link_ipv4 & policy->link_mask))
        {
            return false;
        }
    }
    return true;
}
