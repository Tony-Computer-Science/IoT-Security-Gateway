#include "gateway_uart.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t raw[GW_FRAME_SIZE], output[GW_FRAME_SIZE];
static uint8_t encoded[GW_UART_ENCODED_MAX];

static gw_uart_result_t feed(gw_uart_stream_t *stream, const uint8_t *bytes, size_t size)
{
    gw_uart_result_t result = GW_UART_PENDING;
    for (size_t i = 0; i < size; ++i)
    {
        result = gw_uart_feed(stream, bytes[i], output);
        if (i + 1 < size)
        {
            assert(result == GW_UART_PENDING);
        }
    }
    return result;
}

void test_gateway_uart(void)
{
    gw_uart_stream_t stream = {0};
    /* Exercise long nonzero runs, all-zero padding, every byte value and
     * deterministic random data; UART delivery is deliberately bytewise. */
    uint32_t random = 12345;
    for (unsigned pattern = 0; pattern < 100; ++pattern)
    {
        for (size_t i = 0; i < sizeof(raw); ++i)
        {
            random = random * 1664525u + 1013904223u;
            raw[i] = pattern == 0 ? 0 : pattern == 1 ? 255 : pattern == 2 ? i : random >> 24;
        }
        size_t size = gw_uart_encode(encoded, raw);
        assert(size <= sizeof(encoded) && encoded[size - 1] == 0);
        for (size_t i = 0; i + 1 < size; ++i)
        {
            assert(encoded[i]);
        }
        assert(feed(&stream, encoded, size) == GW_UART_FRAME);
        assert(!memcmp(raw, output, sizeof(raw)));
    }
    gw_frame_t frame = {.type = GW_HELLO, .sender = 1, .sequence = 1}, decoded;
    assert(gw_encode(raw, &frame));
    size_t size = gw_uart_encode(encoded, raw);
    assert(size < 100); /* Control frames must not consume 1600 serial bytes. */
    /* Truncation, reboot delimiters and noise cannot prevent the next frame. */
    assert(feed(&stream, encoded, size / 2) == GW_UART_PENDING);
    assert(gw_uart_feed(&stream, 0, output) == GW_UART_INVALID);
    assert(gw_uart_feed(&stream, 0, output) == GW_UART_PENDING);
    assert(feed(&stream, encoded, size) == GW_UART_FRAME);
    assert(gw_decode(output, sizeof(output), &decoded));
    assert(decoded.sender == 1);
    /* More than a maximum frame without delimiters stays bounded. */
    for (size_t i = 0; i < 10000; ++i)
    {
        assert(gw_uart_feed(&stream, 255, output) == GW_UART_PENDING);
    }
    assert(stream.discard && stream.length <= sizeof(stream.encoded));
    assert(gw_uart_feed(&stream, 0, output) == GW_UART_INVALID);
    assert(feed(&stream, encoded, size) == GW_UART_FRAME);
    /* Invalid COBS block length and oversized decoded output are rejected. */
    const uint8_t bad[] = {255, 1, 0};
    assert(feed(&stream, bad, sizeof(bad)) == GW_UART_INVALID);
    for (size_t i = 0; i < GW_FRAME_SIZE + 2; ++i)
    {
        assert(gw_uart_feed(&stream, 1, output) == GW_UART_PENDING);
    }
    assert(gw_uart_feed(&stream, 0, output) == GW_UART_INVALID);
    assert(feed(&stream, encoded, size) == GW_UART_FRAME);
    /* A full MTU Ethernet payload may itself end in zero bytes. Restore
     * those bytes as well as SPI padding before checking the original CRC. */
    uint8_t ethernet[GW_ETH_MAX] = {0};
    ethernet[12] = 8;
    ethernet[14] = 0x45;
    ethernet[16] = 1500 >> 8;
    ethernet[17] = 1500 & 255;
    memset(ethernet + 34, 0x5a, sizeof(ethernet) - 34 - 50);
    frame.type = GW_DATA;
    frame.peer = 2;
    frame.payload = ethernet;
    frame.length = sizeof(ethernet);
    assert(gw_encode(raw, &frame));
    size = gw_uart_encode(encoded, raw);
    assert(size < GW_FRAME_SIZE);
    assert(feed(&stream, encoded, size) == GW_UART_FRAME);
    assert(!memcmp(raw, output, sizeof(raw)));
    assert(gw_decode(output, sizeof(output), &decoded));
    assert(decoded.type == GW_DATA && decoded.length == GW_ETH_MAX);
    /* Valid COBS does not bypass the inner CRC. */
    raw[100] ^= 1;
    size = gw_uart_encode(encoded, raw);
    assert(feed(&stream, encoded, size) == GW_UART_FRAME);
    assert(!gw_decode(output, sizeof(output), &decoded));
    puts("PASS UART: binary round trips, bounded overflow, truncation/resync and inner CRC");
}
