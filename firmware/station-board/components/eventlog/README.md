# eventlog

Legacy scaffold from the former onboard API design. It is not started; the
current application event log and `GET /api/events` belong to the separate
web service outside the boards. See
[Setting Up](../../../../README.md#setting-up).

**Requirements:** R5

## Current owner (provisional)

Lal Za Lian. Any member may work here; the current owner reviews. Reassign by
updating this section and any CODEOWNERS file added later in the same PR.

## Status

Unused stub. `eventlog_init()` returns `ESP_ERR_NOT_SUPPORTED`. The retained
historical API is in `include/eventlog.h`.

## Historical design notes

These describe the former proposal, not implemented firmware behavior.

- Bounded: capacity is TODO and must fit alongside mbedTLS buffers.
- Entries must never contain credentials or tokens (R2).
- Event shape: see `GET /api/events` in [docs/api-contract.md](../../../../docs/api-contract.md).

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
