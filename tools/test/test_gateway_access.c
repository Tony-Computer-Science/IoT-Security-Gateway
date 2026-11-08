/* Board A policy tests execute the same pure C code linked into the firmware. */
#include "gateway_access.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const gw_access_network_t network = {
    .ap_ip = 0xc0a84e01, .spi_ip = 0xc0a84d02, .spi_peer = 0xc0a84d01, .spi_mask = 0xfffffffc};

static void write_ip(uint8_t *bytes, uint32_t ip)
{
    bytes[0] = ip >> 24;
    bytes[1] = ip >> 16;
    bytes[2] = ip >> 8;
    bytes[3] = ip;
}

static gw_status_t ready_bridge(void)
{
    gw_status_t status = {.flags = GW_IPV4 | GW_FORWARDING};
    write_ip(status.ip, 0xc0a80164);
    write_ip(status.mask, 0xffffff00);
    write_ip(status.link_local, network.spi_peer);
    write_ip(status.link_peer, network.spi_ip);
    write_ip(status.link_mask, network.spi_mask);
    write_ip(status.dns[0], 0xc0a80101);
    write_ip(status.dns[1], 0x01010101);
    return status;
}

static void address_and_readiness(void)
{
    assert(gw_access_address_plan_valid(100, 199, 10, 4));
    assert(gw_access_address_plan_valid(100, 199, 10, 10));
    assert(gw_access_address_plan_valid(253, 254, 10, 2));
    assert(!gw_access_address_plan_valid(100, 199, 100, 4));
    assert(!gw_access_address_plan_valid(100, 199, 150, 4));
    assert(!gw_access_address_plan_valid(100, 199, 199, 4));
    assert(!gw_access_address_plan_valid(100, 199, 1, 4));
    assert(!gw_access_address_plan_valid(100, 199, 255, 4));
    assert(!gw_access_address_plan_valid(1, 100, 200, 4));
    assert(!gw_access_address_plan_valid(100, 255, 10, 4));
    assert(!gw_access_address_plan_valid(2, 102, 200, 4));
    assert(!gw_access_address_plan_valid(100, 100, 10, 1));
    assert(!gw_access_address_plan_valid(199, 100, 10, 4));
    assert(!gw_access_address_plan_valid(100, 102, 10, 4));
    assert(!gw_access_address_plan_valid(100, 199, 10, 0));
    assert(!gw_access_address_plan_valid(100, 199, 10, 11));
    assert(gw_access_config_valid(&network));
    gw_access_network_t invalid = network;
    invalid.ap_ip = network.spi_peer;
    assert(!gw_access_config_valid(&invalid));
    invalid.ap_ip = 0x08080801; /* Public subnets are not AP configuration. */
    assert(!gw_access_config_valid(&invalid));
    invalid.ap_ip = 0x0a000002;
    assert(!gw_access_config_valid(&invalid));
    invalid.ap_ip = 0x0a000001;
    assert(gw_access_config_valid(&invalid));
    invalid.ap_ip = 0xac100001;
    assert(gw_access_config_valid(&invalid));

    gw_status_t status = ready_bridge();
    assert(gw_access_bridge_state(NULL, &network) == GW_ACCESS_OFFLINE);
    for (unsigned flags = 0; flags < 32; ++flags)
    {
        status.flags = flags;
        bool ready = (flags & (GW_IPV4 | GW_FORWARDING)) == (GW_IPV4 | GW_FORWARDING) &&
                     !(flags & (GW_OVERLAP | GW_CONFIG_ERROR));
        assert((gw_access_bridge_state(&status, &network) == GW_ACCESS_READY) == ready);
    }
    status = ready_bridge();
    status.link_peer[3] = 3;
    assert(gw_access_bridge_state(&status, &network) == GW_ACCESS_ADDRESS_MISMATCH);
    status = ready_bridge();
    status.link_mask[3] = 0;
    assert(gw_access_bridge_state(&status, &network) == GW_ACCESS_ADDRESS_MISMATCH);
    status = ready_bridge();
    write_ip(status.ip, network.ap_ip + 1);
    assert(gw_access_bridge_state(&status, &network) == GW_ACCESS_SUBNET_OVERLAP);
    status = ready_bridge();
    write_ip(status.mask, 0xffff0000); /* Broad upstream /16 overlaps both private links. */
    assert(gw_access_bridge_state(&status, &network) == GW_ACCESS_SUBNET_OVERLAP);
    write_ip(status.mask, 0xff00ff00);
    assert(gw_access_bridge_state(&status, &network) == GW_ACCESS_OFFLINE);
    status = ready_bridge();
    assert(gw_access_bridge_state(&status, &network) == GW_ACCESS_READY); /* recovery */

    uint32_t dns[2];
    gw_access_dns(&status, &network, dns);
    assert(dns[0] == 0xc0a80101 && dns[1] == 0x01010101);
    write_ip(status.dns[0], network.ap_ip);
    gw_access_dns(&status, &network, dns);
    assert(dns[0] == 0x01010101 && dns[1] == 0);
    write_ip(status.dns[1], network.spi_peer);
    gw_access_dns(&status, &network, dns);
    assert(!dns[0] && !dns[1]);
    write_ip(status.dns[0], 0x01010101);
    write_ip(status.dns[1], 0x01010101);
    gw_access_dns(&status, &network, dns);
    assert(dns[0] == 0x01010101 && !dns[1]);
}

static void packet_policy(void)
{
    uint8_t packet[40] = {0x45, 0, 0, 40, 0, 0, 0x40, 0, 64, 6};
    packet[32] = 0x50;
    write_ip(packet + 12, network.ap_ip + 1);
    write_ip(packet + 16, 0x08080808);
#define ALLOWED(clients, ready)                                                                    \
    gw_access_ip4_allowed(packet, sizeof(packet), sizeof(packet), &network, clients, ready)
    assert(ALLOWED(true, true));
    assert(!ALLOWED(true, false));
    const unsigned client_hosts[] = {10, 100, 101, 199};
    packet[33] = 0x02;
    for (unsigned i = 0; i < sizeof(client_hosts) / sizeof(client_hosts[0]); ++i)
    {
        write_ip(packet + 12, (network.ap_ip & GW_AP_MASK) | client_hosts[i]);
        assert(ALLOWED(true, true)); /* Static UI and multiple DHCP clients can connect out. */
        assert(!ALLOWED(true, false));
    }
    packet[33] = 0;
    write_ip(packet + 12, network.spi_ip);
    assert(!ALLOWED(true, true)); /* Spoofed client source. */
    write_ip(packet + 12, network.ap_ip);
    assert(!ALLOWED(true, true)); /* The gateway cannot be impersonated. */
    write_ip(packet + 12, network.ap_ip + 1);
    write_ip(packet + 16, network.spi_peer);
    assert(!ALLOWED(true, true)); /* No client route to infrastructure addresses. */
    write_ip(packet + 16, network.ap_ip + 2);
    assert(!ALLOWED(true, true)); /* No same-subnet IP routing via A. */

    write_ip(packet + 12, 0x08080808);
    write_ip(packet + 16, network.spi_ip);
    assert(ALLOWED(false, true)); /* Pre-NAT return destination. */
    packet[33] = 0x02;
    assert(!ALLOWED(false, true)); /* Outside cannot open a TCP connection. */
    packet[33] = 0x12;
    assert(ALLOWED(false, true)); /* SYN+ACK replies to client connections work. */
    packet[33] = 0x10;
    assert(ALLOWED(false, true)); /* Established reply traffic remains usable. */
    packet[33] = 0;
    assert(!ALLOWED(false, false));
    write_ip(packet + 16, network.ap_ip + 1);
    assert(!ALLOWED(false, true)); /* Direct route bypass from B. */
    write_ip(packet + 16, network.ap_ip);
    assert(!ALLOWED(false, true));

    packet[9] = 17;
    packet[20] = 0;
    packet[21] = 68;
    packet[22] = 0;
    packet[23] = 67;
    packet[24] = 0;
    packet[25] = 20;
    write_ip(packet + 12, 0);
    write_ip(packet + 16, UINT32_MAX);
    assert(ALLOWED(true, false)); /* DHCP discover before a lease or upstream. */
    write_ip(packet + 12, network.ap_ip + 1);
    write_ip(packet + 16, network.ap_ip);
    assert(ALLOWED(true, false)); /* DHCP renew. */
    packet[21] = 69;
    assert(!ALLOWED(true, false)); /* The DHCP exception is port-specific. */
    packet[21] = 68;
    write_ip(packet + 16, 0x08080808);
    assert(!ALLOWED(true, false)); /* It cannot bypass the forwarding gate. */
    packet[25] = 7;
    assert(!ALLOWED(true, true));
    packet[25] = 20;

    packet[9] = 1;
    packet[20] = 8;
    packet[21] = 0;
    write_ip(packet + 16, network.ap_ip);
    assert(ALLOWED(true, false)); /* Local gateway diagnostics remain usable. */
    packet[20] = 3;
    assert(!ALLOWED(true, false));
    packet[20] = 8;
    packet[6] = 0x20;
    assert(!ALLOWED(true, true)); /* No fragment can reach NAPT's L4 parser. */
    packet[6] = 0x40;
    for (size_t length = 0; length < sizeof(packet); ++length)
    {
        assert(!gw_access_ip4_allowed(packet, length, length, &network, true, true));
    }
#undef ALLOWED
}

void test_gateway_access(void)
{
    address_and_readiness();
    packet_policy();
    puts("PASS Board A: configuration, bridge readiness, DNS, offline DHCP, spoofing and NAT "
         "bypass");
}
