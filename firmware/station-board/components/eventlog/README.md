# eventlog

A fixed-capacity ring buffer of events (auth outcomes, rejected commands, rate-limit events, target failures), read through `GET /api/events`.

**Requirements:** R5

## Current owner (provisional)

Lal Za Lian. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `eventlog_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/eventlog.h`; expand it as the design firms up.

## Notes

- Bounded: capacity is TODO and must fit alongside mbedTLS buffers.
- Entries must never contain credentials or tokens (R2).
- Event shape: see `GET /api/events` in [docs/api-contract.md](../../../../docs/api-contract.md).

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
