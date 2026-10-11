# spi_host

The SPI **controller** side of the only link between Board A and Board B. It encodes allow-listed operations into frames, sends them, decodes responses, and tracks link readiness for `link_ok`.

**Requirements:** R7, R3

## Current owner (provisional)

Zachariah Grande / Carter Cattadoris (shared). Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `spi_host_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/spi_host.h`; expand it as the design firms up.

## Notes

- Protocol: [docs/spi-protocol.md](../../../../docs/spi-protocol.md) (stub, nothing decided).
- Pins: TODO, defined by `hardware/docs/`. Do not hardcode guesses.
- Counterpart: `firmware/bridge-board/components/spi_device`.

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
