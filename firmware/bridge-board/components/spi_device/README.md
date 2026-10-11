# spi_device

The SPI **peripheral** side of the inter-board link. It receives frames from Board A, validates them strictly, and hands only well-formed, allow-listed operations to `target_client`.

**Requirements:** R3, R7

## Current owner (provisional)

Zachariah Grande. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `spi_device_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/spi_device.h`; expand it as the design firms up.

## Notes

- Reject any frame that fails validation. Never forward raw bytes.
- Protocol: [docs/spi-protocol.md](../../../../docs/spi-protocol.md).
- Counterpart: `firmware/station-board/components/spi_host`.

## Build

Built as part of the `bridge-board` ESP-IDF project (`idf.py build` in
`firmware/bridge-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
