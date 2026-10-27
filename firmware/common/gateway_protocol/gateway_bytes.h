#pragma once
#include <stdint.h>

/* Byte-wise access works with unaligned buffers and any host endianness. */
static inline uint16_t read_u16_le(const uint8_t *bytes)
{
    return bytes[0] | (uint16_t)bytes[1] << 8;
}
static inline uint32_t read_u32_le(const uint8_t *bytes)
{
    return read_u16_le(bytes) | (uint32_t)read_u16_le(bytes + 2) << 16;
}
static inline uint64_t read_u64_le(const uint8_t *bytes)
{
    return read_u32_le(bytes) | (uint64_t)read_u32_le(bytes + 4) << 32;
}
static inline void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = value;
    bytes[1] = value >> 8;
}
static inline void write_u32_le(uint8_t *bytes, uint32_t value)
{
    write_u16_le(bytes, value);
    write_u16_le(bytes + 2, value >> 16);
}
static inline void write_u64_le(uint8_t *bytes, uint64_t value)
{
    write_u32_le(bytes, value);
    write_u32_le(bytes + 4, value >> 32);
}
static inline uint16_t read_u16_be(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] << 8 | bytes[1];
}
static inline uint32_t read_u32_be(const uint8_t *bytes)
{
    return (uint32_t)read_u16_be(bytes) << 16 | read_u16_be(bytes + 2);
}
