# firmware/station-board: Board A

The user-facing half of the gateway. It joins the existing Wi-Fi network,
exposes HTTPS on port 443 (the only service reachable from the user network),
authenticates users, enforces the command allow-list and rate limits, keeps
the event log, serves the web interface, and is the **SPI controller** for the
link to Board B.

## Current owner (provisional)

Two members share this project. The split into components keeps their work in
separate source files:

| Component | Purpose | Current owner (provisional) |
|---|---|---|
| [`net_station`](components/net_station/) | Wi-Fi station + HTTPS server, web asset serving | Antonio (Tony) Goncalves |
| [`auth`](components/auth/) | Login, sessions, token checks | Antonio (Tony) Goncalves |
| [`policy`](components/policy/) | Allow-list, input validation, rate limiting | Lal Za Lian |
| [`eventlog`](components/eventlog/) | Bounded event log | Lal Za Lian |
| [`spi_host`](components/spi_host/) | SPI controller for the inter-board link | Zachariah Grande / Carter Cattadoris |
| `main/` | Boot and wiring only | Antonio (Tony) Goncalves (project lead) |

Any member may work in any component; the listed owner reviews.

## Layout

```
station-board/
├── CMakeLists.txt         project file
├── sdkconfig.defaults     tracked defaults (target, flash size, partition table). NO secrets.
├── partitions.csv         nvs, phy_init, factory app, www (LittleFS web assets)
├── main/main.c            boots and logs a banner; nothing else yet
└── components/            one directory per area (see table above)
```

## Build

```sh
cd firmware/station-board
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

Expected serial output after boot:

```
I (...) station_board: ========================================
I (...) station_board:  IoT Security Gateway: BOARD A (station board)
...
```

## Web assets

The web UI is built separately (`python3 web/scripts/build.py`) and packed into
the `www` partition (`web/scripts/mklittlefs.sh`). See [web/README.md](../../web/README.md).

## Status / TODO

- Every component is a stub (`*_init()` returns `ESP_ERR_NOT_SUPPORTED`), and
  `main.c` does not call them yet.
- `partitions.csv` sizes and `sdkconfig.defaults` flash size are provisional
  (TODOs in those files).
- The `esp_littlefs` managed component is not added yet.
