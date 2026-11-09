#include "target_network_policy.h"

bool target_ipv4_allowed(const uint8_t *packet, size_t length, uint32_t pc_ipv4)
{
    if (!packet || length < 20 || (packet[0] >> 4) != 4)
    {
        return false;
    }
    size_t header = (packet[0] & 15u) * 4u;
    if (header < 20 || header > length)
    {
        return false;
    }
    uint32_t source = ((uint32_t)packet[12] << 24) | ((uint32_t)packet[13] << 16) |
                      ((uint32_t)packet[14] << 8) | packet[15];
    if (pc_ipv4 && source == pc_ipv4)
    {
        return true;
    }
    /* Permit unfragmented ping on the AP's /24 without opening application
     * traffic. lwIP still validates lengths/checksums and handles ICMP. */
    if ((packet[6] & 0x3f) || packet[7] || length < header + 8)
    {
        return false;
    }
    if (packet[9] == 1 && pc_ipv4 && (source >> 8) == (pc_ipv4 >> 8) &&
        (source & 255u) != 0 && (source & 255u) != 255 &&
        (packet[header] == 0 || packet[header] == 8) && packet[header + 1] == 0)
    {
        return true;
    }
    /* DHCP replies are the only non-PC UDP exception. */
    return packet[9] == 17 && packet[header] == 0 && packet[header + 1] == 67 &&
           packet[header + 2] == 0 && packet[header + 3] == 68;
}
