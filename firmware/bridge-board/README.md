# firmware/bridge-board: Board B

The target-facing half of the gateway. It is the **SPI peripheral** on the
wired link from Board A, hosts the private Wi-Fi access point the target joins,
and re-issues validated operations to the single fixed target address.

Board B has no route to the user network. It never forwards packets, bridges,
or NATs. The only input it acts on is a valid SPI frame.

## Current owner (provisional)

Zachariah Grande. Any member may work here; the current owner reviews.

| Component | Purpose |
|---|---|
| [`spi_device`](components/spi_device/) | SPI peripheral, strict frame validation |
| [`ap_mode`](components/ap_mode/) | Private Wi-Fi AP for the target |
| [`target_client`](components/target_client/) | HTTP client to the fixed target address |

## Build

```sh
cd firmware/bridge-board
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

## Status / TODO

- All components are stubs. `main.c` only logs a banner.
- The SPI protocol is not designed yet: [docs/spi-protocol.md](../../docs/spi-protocol.md).
- Pin assignments come from `hardware/docs/` (TODO).
- TODO: the fixed target address and AP SSID/password come from `menuconfig`
  (git-ignored `sdkconfig`), never from committed files.
