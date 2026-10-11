# policy

Decides whether a request may proceed. It holds the command allow-list (`led_on`, `led_off`), validates input (method, path, body size, JSON shape, values), and enforces the configurable limits on failed logins and command rate.

**Requirements:** R3, R4

## Current owner (provisional)

Lal Za Lian. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `policy_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/policy.h`; expand it as the design firms up.

## Notes

- Limits must be configurable (Kconfig suggested). The values are TODO, see API contract Q4.
- Rejection must happen before anything is sent over SPI, so rejected requests leave the target unchanged (R6).

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
