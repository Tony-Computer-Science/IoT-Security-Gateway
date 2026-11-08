#include "gateway_uart.h"
#include <string.h>

size_t gw_uart_encode(uint8_t *output, const uint8_t frame[GW_FRAME_SIZE])
{
    /* SPI needs fixed-size transfers; a serial stream need not carry their
     * trailing zero padding. The receiver restores it before inner CRC checks. */
    size_t frame_length = GW_FRAME_SIZE;
    while (frame_length > GW_HEADER_SIZE && frame[frame_length - 1] == 0)
    {
        --frame_length;
    }
    size_t write = 1, code_at = 0;
    uint8_t code = 1;
    for (size_t i = 0; i < frame_length; ++i)
    {
        if (frame[i] == 0)
        {
            output[code_at] = code;
            code_at = write++;
            code = 1;
        }
        else
        {
            output[write++] = frame[i];
            if (++code == 255)
            {
                output[code_at] = code;
                code_at = write++;
                code = 1;
            }
        }
    }
    output[code_at] = code;
    output[write++] = 0;
    return write;
}

static bool decode(const uint8_t *input, size_t length, uint8_t *output)
{
    size_t read = 0, write = 0;
    while (read < length)
    {
        uint8_t code = input[read++];
        size_t count = code - 1u;
        if (!code || count > length - read || count > GW_FRAME_SIZE - write)
        {
            return false;
        }
        memcpy(output + write, input + read, count);
        read += count;
        write += count;
        if (code != 255 && read < length)
        {
            if (write == GW_FRAME_SIZE)
            {
                return false;
            }
            output[write++] = 0;
        }
    }
    if (write < GW_HEADER_SIZE)
    {
        return false;
    }
    memset(output + write, 0, GW_FRAME_SIZE - write);
    return true;
}

gw_uart_result_t gw_uart_feed(gw_uart_stream_t *stream, uint8_t byte, uint8_t output[GW_FRAME_SIZE])
{
    if (byte)
    {
        if (!stream->discard)
        {
            if (stream->length == sizeof(stream->encoded))
            {
                stream->discard = true;
            }
            else
            {
                stream->encoded[stream->length++] = byte;
            }
        }
        return GW_UART_PENDING;
    }
    gw_uart_result_t result = GW_UART_PENDING;
    if (stream->discard || stream->length)
    {
        result = !stream->discard && decode(stream->encoded, stream->length, output)
                     ? GW_UART_FRAME
                     : GW_UART_INVALID;
    }
    stream->length = 0;
    stream->discard = false;
    return result;
}
