# Firmware

| Project | Role | Status |
|---|---|---|
| station-board | A, WPA2 AP/client DHCP/NAT to SPI or UART | Implemented; basic C6 UART hardware checks passed, full acceptance pending |
| bridge-board | B, upstream Wi-Fi station, SPI/UART endpoint, IPv4 NAPT | Implemented; basic C6 UART hardware checks passed, full acceptance pending |
| spi-test-host | Temporary A image, SPI controller and network tests | Implemented; Wi-Fi never initialized |
| target-device | C6/S3 Wi-Fi LED target C, outbound polling to UI PC | Implemented; S3 bench acceptance recorded, C6 fixed-IP PC acceptance pending |
| common | Portable protocol, SPI/UART driver, identity LED and optional diagnostics | Protocol/link used by A, B and test host; LED/diagnostics used by A/B |

Station and bridge use ESP-IDF **6.1** and support ESP32-S3 (Rev A PCB/default)
or ESP32-C6 (development boards with the separate
[C6 pinout](../hardware/docs/README.md#esp32-c6-development-board-wiring)).
The test-host image still targets ESP32-S3. Board C now uses ESP-IDF 6.1 and
ESP32-C6 or ESP32-S3; it joins the station AP as an ordinary Wi-Fi client and uses the
[PC UI service](../web/README.md#control-the-third-c6).
See each project's README for commands. Station and bridge can also use a
[two-wire UART test link](../docs/uart-link.md) instead of SPI. SSIDs and
passwords belong in git-ignored sdkconfig, never tracked defaults. Generated
binaries also contain configured credentials and must not be published.

The [architecture](../docs/architecture.md) explains the intentional change
from command-only isolation to outbound IPv4 routing. The SPI test host remains
a separate diagnostic image without Wi-Fi; flash station-board onto A to use clients.
See [recorded UART results](../docs/uart-link.md#observed-hardware-checks-2026-11-05)
and the [remaining acceptance checks](../docs/testing.md).
