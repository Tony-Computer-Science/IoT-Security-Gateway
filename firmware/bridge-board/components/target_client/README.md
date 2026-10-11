# target_client

An HTTP client that issues requests to the single fixed target address compiled into the firmware. It maps target responses and failures back to SPI result frames.

**Requirements:** R3 (clients cannot choose a destination), R6

## Current owner (provisional)

Zachariah Grande. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `target_client_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/target_client.h`; expand it as the design firms up.

## Notes

- The destination is fixed in firmware and never taken from a frame.
- Timeout must be shorter than Board A's SPI transaction timeout (see spi-protocol.md §6).

## Build

Built as part of the `bridge-board` ESP-IDF project (`idf.py build` in
`firmware/bridge-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
