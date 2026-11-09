#include "target_network_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const uint32_t pc = 0xc0a84e0a;
    uint8_t packet[68] = {0x45};
    packet[9] = 6;
    packet[12] = 192; packet[13] = 168; packet[14] = 78; packet[15] = 10;
    assert(target_ipv4_allowed(packet, 40, pc));
    assert(!target_ipv4_allowed(packet, 19, pc));
    assert(!target_ipv4_allowed(NULL, 40, pc));
    packet[0] = 0x65;
    assert(!target_ipv4_allowed(packet, 40, pc));
    packet[0] = 0x4f;
    assert(!target_ipv4_allowed(packet, 40, pc));
    packet[0] = 0x45;
    packet[15] = 101;
    assert(!target_ipv4_allowed(packet, 40, pc));
    packet[9] = 1;
    packet[20] = 8;
    assert(target_ipv4_allowed(packet, 28, pc));
    assert(!target_ipv4_allowed(packet, 27, pc));
    packet[20] = 0;
    assert(target_ipv4_allowed(packet, 28, pc));
    packet[20] = 3;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[20] = 8; packet[21] = 1;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[21] = 0; packet[14] = 79;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[14] = 78; packet[15] = 255;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[15] = 101; packet[6] = 0x20;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[6] = 0;
    packet[9] = 17;
    packet[20] = 0; packet[21] = 67; packet[22] = 0; packet[23] = 68;
    assert(target_ipv4_allowed(packet, 28, pc));
    assert(!target_ipv4_allowed(packet, 27, pc));
    packet[6] = 0x20;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[6] = 0;
    packet[7] = 1;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[7] = 0;
    packet[23] = 69;
    assert(!target_ipv4_allowed(packet, 28, pc));
    packet[23] = 68;
    packet[0] = 0x46;
    assert(!target_ipv4_allowed(packet, 28, pc));
    memmove(packet + 24, packet + 20, 8);
    assert(target_ipv4_allowed(packet, 32, pc));
    puts("PASS target IPv4 isolation: PC, peer application denial, local ping, DHCP and malformed input");
}
