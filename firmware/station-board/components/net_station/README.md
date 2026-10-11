# net_station

Joins the existing Wi-Fi network as a station and runs the HTTPS server on port 443, the gateway's only exposed service. It routes `/api/*` requests to the REST handlers and serves the gzipped web assets from the `www` LittleFS partition.

**Requirements:** R1, R2 (encrypted connection), R4 (connection counts, buffers, timeouts)

## Current owner (provisional)

Antonio (Tony) Goncalves. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `net_station_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/net_station.h`; expand it as the design firms up.

## Notes

- TLS certificate and key are generated locally and must never be committed (`.gitignore` blocks `*.pem`, `*.key`, `*.crt`).
- Wi-Fi SSID and password come from `menuconfig` (git-ignored `sdkconfig`), not `sdkconfig.defaults`.
- Contract: [docs/api-contract.md](../../../../docs/api-contract.md).

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
