/* Wire serialization, corruption detection and Ethernet shape checks. */
#include "gateway_protocol.h"
#include "gateway_bytes.h"
#include <string.h>

static uint32_t crc_byte(uint32_t crc, uint8_t value)
{
    crc ^= value;
    for (unsigned i = 0; i < 8; ++i)
    {
        crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}
uint32_t gw_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i)
    {
        crc = crc_byte(crc, data[i]);
    }
    return ~crc;
}
/* The checksum field is logically zero while calculating a wire checksum. */
static uint32_t frame_crc(const uint8_t *data)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < GW_FRAME_SIZE; ++i)
    {
        crc = crc_byte(crc, i >= 32 && i < 36 ? 0 : data[i]);
    }
    return ~crc;
}
bool gw_ethernet_valid(const uint8_t *data, size_t length)
{
    if (!data || length < 14 || length > GW_ETH_MAX)
    {
        return false;
    }
    uint16_t type = (uint16_t)data[12] << 8 | data[13];
    if (type == 0x0806)
    {
        return length >= 42 && data[14] == 0 && data[15] == 1 && data[16] == 8 && data[17] == 0 &&
               data[18] == 6 && data[19] == 4 && data[20] == 0 && (data[21] == 1 || data[21] == 2);
    }
    if (type != 0x0800 || length < 34 || data[14] >> 4 != 4)
    {
        return false;
    }
    size_t ip_header_length = (data[14] & 15) * 4;
    size_t ip_length = (size_t)data[16] << 8 | data[17];
    return ip_header_length >= 20 && ip_length >= ip_header_length && ip_length <= length - 14;
}
static bool frame_shape_valid(const gw_frame_t *frame)
{
    if (!frame->sender || !frame->sequence || (frame->flags & ~GW_MORE) ||
        frame->length > GW_ETH_MAX || (frame->length && !frame->payload))
    {
        return false;
    }
    switch (frame->type)
    {
    case GW_HELLO:
        return frame->length == 0;
    case GW_IDLE:
        return frame->length == 0 && frame->peer;
    case GW_STATUS:
        return frame->length == GW_STATUS_SIZE && frame->peer;
    case GW_DATA:
        return frame->peer && gw_ethernet_valid(frame->payload, frame->length);
    default:
        return false;
    }
}
bool gw_encode(uint8_t output[GW_FRAME_SIZE], const gw_frame_t *frame)
{
    if (!output || !frame || !frame_shape_valid(frame))
    {
        return false;
    }
    memset(output, 0, GW_FRAME_SIZE);
    memcpy(output, "IGW2", 4);
    output[4] = GW_WIRE_VERSION;
    output[5] = frame->type;
    write_u16_le(output + 6, frame->flags);
    write_u16_le(output + 8, frame->length);
    write_u32_le(output + 12, frame->sequence);
    write_u64_le(output + 16, frame->sender);
    write_u64_le(output + 24, frame->peer);
    if (frame->length)
    {
        memcpy(output + GW_HEADER_SIZE, frame->payload, frame->length);
    }
    write_u32_le(output + 32, frame_crc(output));
    return true;
}
bool gw_decode(const uint8_t *data, size_t length, gw_frame_t *frame)
{
    if (!data || !frame || length != GW_FRAME_SIZE || memcmp(data, "IGW2", 4) ||
        data[4] != GW_WIRE_VERSION || read_u16_le(data + 10) || read_u32_le(data + 36) ||
        read_u32_le(data + 32) != frame_crc(data))
    {
        return false;
    }
    *frame = (gw_frame_t){.type = data[5],
                          .flags = read_u16_le(data + 6),
                          .length = read_u16_le(data + 8),
                          .sequence = read_u32_le(data + 12),
                          .sender = read_u64_le(data + 16),
                          .peer = read_u64_le(data + 24),
                          .payload = data + GW_HEADER_SIZE};
    if (!frame_shape_valid(frame))
    {
        return false;
    }
    for (size_t i = GW_HEADER_SIZE + frame->length; i < GW_FRAME_SIZE; ++i)
    {
        if (data[i])
        {
            return false;
        }
    }
    return true;
}
