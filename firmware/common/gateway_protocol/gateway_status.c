/* Fixed-offset status payload. IP arrays remain network-order octets.
 * Integer counters are explicitly serialized; never memcpy the C struct. */
#include "gateway_protocol.h"
#include "gateway_bytes.h"
#include <string.h>

void gw_status_encode(uint8_t output[GW_STATUS_SIZE], const gw_status_t *status)
{
    write_u32_le(output, status->flags);
    memcpy(output + 4, status->ip, 4);
    memcpy(output + 8, status->mask, 4);
    memcpy(output + 12, status->gateway, 4);
    memcpy(output + 16, status->dns, 8);
    memcpy(output + 24, status->link_local, 4);
    memcpy(output + 28, status->link_peer, 4);
    memcpy(output + 32, status->link_mask, 4);
    write_u32_le(output + 36, status->reason);
    write_u32_le(output + 40, status->retry_ms);
    write_u32_le(output + 44, status->received);
    write_u32_le(output + 48, status->rejected);
    write_u32_le(output + 52, status->dropped);
}
void gw_status_decode(gw_status_t *status, const uint8_t input[GW_STATUS_SIZE])
{
    memset(status, 0, sizeof(*status));
    status->flags = read_u32_le(input);
    memcpy(status->ip, input + 4, 4);
    memcpy(status->mask, input + 8, 4);
    memcpy(status->gateway, input + 12, 4);
    memcpy(status->dns, input + 16, 8);
    memcpy(status->link_local, input + 24, 4);
    memcpy(status->link_peer, input + 28, 4);
    memcpy(status->link_mask, input + 32, 4);
    status->reason = read_u32_le(input + 36);
    status->retry_ms = read_u32_le(input + 40);
    status->received = read_u32_le(input + 44);
    status->rejected = read_u32_le(input + 48);
    status->dropped = read_u32_le(input + 52);
}
