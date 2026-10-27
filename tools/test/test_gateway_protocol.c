#include "gateway_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t wire[GW_FRAME_SIZE], copy[GW_FRAME_SIZE];
static uint8_t ethernet[GW_ETH_MAX];
static bool packet_allowed(const uint8_t *packet, size_t length, bool from_spi, bool from_uplink,
                           bool ready)
{
    gw_ipv4_policy_t policy = {
        .ingress = from_spi      ? GW_INGRESS_SPI
                   : from_uplink ? GW_INGRESS_UPLINK
                                 : GW_INGRESS_LOCAL,
        .forwarding_ready = ready,
        .peer_ipv4 = 0xc0a84d02,
        .link_ipv4 = 0xc0a84d01,
        .link_mask = 0xfffffffc,
    };
    return gw_ip4_allowed(packet, length, length, &policy);
}
static void repair_crc(void)
{
    memset(copy + 32, 0, 4);
    uint32_t crc = gw_crc32(copy, sizeof(copy));
    for (int i = 0; i < 4; ++i)
    {
        copy[32 + i] = crc >> (i * 8);
    }
}
static void framing(void)
{
    assert(gw_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926);
    gw_frame_t frame = {.type = GW_HELLO, .sender = 0x1122334455667788ULL, .sequence = 1}, decoded;
    assert(gw_encode(wire, &frame));
    assert(!memcmp(wire, "IGW2\002\001", 6));
    assert(wire[16] == 0x88 && wire[23] == 0x11 && wire[12] == 1);
    assert(gw_decode(wire, sizeof(wire), &decoded));
    assert(decoded.sender == frame.sender && !decoded.peer && !decoded.length);
    assert(!gw_decode(wire, sizeof(wire) - 1, &decoded));
    assert(!gw_decode(wire, sizeof(wire) + 1, &decoded));
    for (size_t i = 0; i < sizeof(wire); ++i)
    {
        memcpy(copy, wire, sizeof(wire));
        copy[i] ^= 1;
        assert(!gw_decode(copy, sizeof(copy), &decoded));
    }
    const size_t invalid[] = {0, 4, 5, 6, 8, 10, 36, 1599};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        memcpy(copy, wire, sizeof(wire));
        copy[invalid[i]] = 0xff;
        repair_crc();
        assert(!gw_decode(copy, sizeof(copy), &decoded));
    }
    memset(ethernet, 0, sizeof(ethernet));
    ethernet[12] = 8;
    ethernet[14] = 0x45;
    ethernet[16] = 1500 >> 8;
    ethernet[17] = 1500 & 255;
    frame.type = GW_DATA;
    frame.peer = 9;
    frame.length = sizeof(ethernet);
    frame.payload = ethernet;
    assert(gw_encode(wire, &frame) && gw_decode(wire, sizeof(wire), &decoded));
    assert(decoded.length == 1514 && !memcmp(decoded.payload, ethernet, 1514));
    frame.length = 1515;
    assert(!gw_encode(wire, &frame));
    frame.length = 1514;
    ethernet[12] = 0x86;
    ethernet[13] = 0xdd;
    assert(!gw_encode(wire, &frame));
    ethernet[12] = 0x81;
    ethernet[13] = 0;
    assert(!gw_encode(wire, &frame));
    puts("PASS framing: CRC reference, every-byte corruption, semantic validation, MTU and "
         "EtherTypes");
}
static void sessions(void)
{
    gw_session_t a, b;
    gw_session_init(&a, 11);
    gw_session_init(&b, 22);
    gw_frame_t hello_a = {.type = GW_HELLO, .sender = 11, .sequence = 1};
    gw_frame_t hello_b = {.type = GW_HELLO, .sender = 22, .sequence = 1};
    assert(gw_session_accept(&a, &hello_b, 100) == GW_ACCEPT && !a.ready);
    assert(gw_session_accept(&b, &hello_a, 100) == GW_ACCEPT && !b.ready);
    hello_a.peer = 22;
    hello_b.peer = 11;
    ++hello_a.sequence;
    ++hello_b.sequence;
    assert(gw_session_accept(&a, &hello_b, 110) == GW_ACCEPT && a.ready);
    assert(gw_session_accept(&b, &hello_a, 110) == GW_ACCEPT && b.ready);
    assert(gw_session_accept(&a, &hello_b, 200) == GW_REJECT);
    assert(a.last_rx_ms == 110);
    assert(!gw_session_expired(&a, 2109) && gw_session_expired(&a, 2110));
    gw_frame_t data = {.type = GW_DATA, .sender = 22, .peer = 11, .sequence = 3};
    assert(gw_session_accept(&a, &data, 210) == GW_ACCEPT);
    data.peer = 99;
    data.sequence = 4;
    assert(gw_session_accept(&a, &data, 220) == GW_REJECT);
    hello_b.sender = 33;
    hello_b.peer = 0;
    hello_b.sequence = 1;
    assert(gw_session_accept(&a, &hello_b, 230) == GW_PEER_CHANGED);
    gw_session_init(&a, 44);
    data.peer = 11;
    assert(gw_session_accept(&a, &data, 240) == GW_REJECT);
    assert(gw_session_accept(&a, &hello_b, 250) == GW_ACCEPT && !a.ready);
    data.sender = 33;
    data.peer = 44;
    assert(gw_session_accept(&a, &data, 260) == GW_REJECT);
    hello_b.peer = 44;
    ++hello_b.sequence;
    assert(gw_session_accept(&a, &hello_b, 270) == GW_ACCEPT && a.ready);
    puts("PASS sessions: mutual handshake, duplicates, exact timeout, one-sided restart, stale "
         "pairing/data");
}
static void status_and_policy(void)
{
    gw_status_t s = {.flags = GW_ASSOCIATED | GW_IPV4,
                     .ip = {192, 168, 1, 5},
                     .dns = {{1, 1, 1, 1}, {8, 8, 8, 8}},
                     .reason = 201,
                     .retry_ms = 30000,
                     .received = 99},
                out;
    uint8_t bytes[GW_STATUS_SIZE];
    gw_status_encode(bytes, &s);
    gw_status_decode(&out, bytes);
    assert(!memcmp(&s, &out, sizeof(s)));
    assert(bytes[4] == 192 && bytes[40] == 0x30 && bytes[41] == 0x75);
    assert(gw_link_addresses_valid(0xc0a84d01, 0xc0a84d02, 0xfffffffc));
    assert(!gw_link_addresses_valid(0xc0a84d00, 0xc0a84d02, 0xfffffffc));
    assert(!gw_link_addresses_valid(0xc0a84d01, 0xc0a84d01, 0xfffffffc));
    assert(!gw_link_addresses_valid(0xc0a84d01, 0xc0a84e02, 0xfffffffc));
    assert(gw_subnets_overlap(0xc0a84d01, 0xfffffffc, 0xc0a80105, 0xffff0000));
    assert(!gw_subnets_overlap(0xc0a84d01, 0xfffffffc, 0xc0a80105, 0xffffff00));
    for (unsigned v = 0; v < 16; ++v)
    {
        assert(gw_can_forward(v & 1, v & 2, v & 4, v & 8) == (v == 3));
    }
    uint8_t ip[40] = {0x45, 0, 0, 40, 0, 0, 0x40, 0, 64, 6};
    ip[12] = 192;
    ip[13] = 168;
    ip[14] = 77;
    ip[15] = 2;
    ip[16] = 1;
    ip[19] = 1;
    ip[32] = 0x50;
#define ALLOWED(spi, wan, ready) packet_allowed(ip, sizeof(ip), spi, wan, ready)
    assert(ALLOWED(true, false, true));
    assert(!ALLOWED(true, false, false)); /* gate closed */
    ip[15] = 3;
    assert(!ALLOWED(true, false, true));
    ip[15] = 2; /* spoofed source */
    ip[6] = 0x20;
    assert(!ALLOWED(true, false, true));
    ip[6] = 0x40; /* fragments */
    ip[32] = 0x10;
    assert(!ALLOWED(true, false, true));
    ip[32] = 0x50; /* short TCP */
    ip[16] = 192;
    ip[17] = 168;
    ip[18] = 77;
    ip[19] = 2;
    assert(!ALLOWED(false, true, true)); /* upstream direct-route bypass */
    ip[18] = 1;
    assert(ALLOWED(false, true, true)); /* upstream address before NAT */
    for (size_t n = 0; n < 40; ++n)
    {
        assert(!packet_allowed(ip, n, true, false, true));
    }

    /* UDP and ICMP must remain usable while malformed transport lengths fail
     * before the SDK's NAPT code reads them. */
    ip[9] = 17;
    ip[24] = 0;
    ip[25] = 8;
    assert(ALLOWED(true, false, true));
    ip[25] = 7;
    assert(!ALLOWED(true, false, true));
    ip[25] = 21;
    assert(!ALLOWED(true, false, true));
    ip[9] = 1;
    ip[20] = 8; /* ICMP echo request */
    assert(ALLOWED(true, false, true));
    ip[3] = 27;
    assert(!ALLOWED(true, false, true));
    ip[3] = 40;
    ip[9] = 47; /* GRE is outside the supported relay protocols. */
    assert(!ALLOWED(true, false, true));
#undef ALLOWED
    puts("PASS status/addressing/firewall: network order, overlap, all gate states, spoofing and "
         "inbound bypass");
}
static void recovery_simulation(void)
{
    gw_session_t endpoints[2];
    uint32_t sequence[2] = {0}, random = 1234;
    uint64_t id = 100;
    gw_session_init(&endpoints[0], ++id);
    gw_session_init(&endpoints[1], ++id);
    for (unsigned tick = 0; tick < 1000; ++tick)
    {
        if (tick == 100 || tick == 300)
        {
            gw_session_init(&endpoints[0], ++id);
            sequence[0] = 0;
        }
        if (tick == 200 || tick == 300)
        {
            gw_session_init(&endpoints[1], ++id);
            sequence[1] = 0;
        }
        for (unsigned from = 0; from < 2; ++from)
        {
            unsigned to = 1 - from;
            if (gw_session_expired(&endpoints[from], tick * 10))
            {
                gw_session_init(&endpoints[from], ++id);
                sequence[from] = 0;
            }
            gw_frame_t frame = {.type = endpoints[from].ready ? GW_IDLE : GW_HELLO,
                                .sequence = ++sequence[from],
                                .sender = endpoints[from].local,
                                .peer = endpoints[from].peer},
                       decoded;
            assert(gw_encode(wire, &frame));
            random = random * 1664525u + 1013904223u;
            if ((tick >= 350 && tick < 600) || random % 5 == 0)
            {
                continue; /* cable loss + random drop */
            }
            assert(gw_decode(wire, sizeof(wire), &decoded));
            if (gw_session_accept(&endpoints[to], &decoded, tick * 10) == GW_PEER_CHANGED)
            {
                gw_session_init(&endpoints[to], ++id);
                sequence[to] = 0;
            }
        }
        if (tick == 99 || tick == 199 || tick == 299 || tick == 349 || tick == 999)
        {
            assert(endpoints[0].ready && endpoints[1].ready);
            assert(endpoints[0].peer == endpoints[1].local &&
                   endpoints[1].peer == endpoints[0].local);
        }
    }
    puts("PASS recovery simulation: packet loss, independent/simultaneous resets, prolonged cable "
         "loss");
}
int main(void)
{
    void test_gateway_access(void);
    framing();
    sessions();
    status_and_policy();
    recovery_simulation();
    test_gateway_access();
    return 0;
}
