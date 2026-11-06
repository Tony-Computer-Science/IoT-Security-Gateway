# policy

Legacy scaffold from the former onboard API design. It is not started; the
current command validation and login/request limits belong to the separate
web service outside the boards. See
[Setting Up](../../../../README.md#setting-up).

**Requirements:** R3, R4

## Current owner (provisional)

Lal Za Lian. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Unused stub. `policy_init()` returns `ESP_ERR_NOT_SUPPORTED`. The retained
historical API is in `include/policy.h`.

## Notes

- Limits must be configurable (Kconfig suggested). The values are TODO, see API contract Q4.
- Rejection must happen before anything is sent over SPI, so rejected requests leave the target unchanged (R6).

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
