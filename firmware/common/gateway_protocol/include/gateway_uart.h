#pragma once

/* UART envelope 0.2.0 for the unchanged gateway wire-version-2 frame. COBS removes
 * zero bytes so a zero delimiter can recover alignment after loss or reset. */
#include "gateway_protocol.h"

#define GW_UART_ENCODED_MAX (GW_FRAME_SIZE + GW_FRAME_SIZE / 254u + 2u)

typedef struct
{
    uint8_t encoded[GW_UART_ENCODED_MAX - 1];
    size_t length;
    bool discard;
} gw_uart_stream_t;

typedef enum
{
    GW_UART_PENDING,
    GW_UART_FRAME,
    GW_UART_INVALID
} gw_uart_result_t;

/* Output capacity must be GW_UART_ENCODED_MAX. Includes the final delimiter.
 * Trailing zero padding is omitted and restored by the stream decoder. */
size_t gw_uart_encode(uint8_t *output, const uint8_t frame[GW_FRAME_SIZE]);
/* Zero-initialize stream. Only GW_UART_FRAME makes output valid; the caller
 * must still validate its CRC, semantics and session with the normal codec. */
gw_uart_result_t gw_uart_feed(gw_uart_stream_t *stream, uint8_t byte,
                              uint8_t output[GW_FRAME_SIZE]);
