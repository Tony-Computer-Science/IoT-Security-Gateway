# Firmware

| Project | Role | Status |
|---|---|---|
| station-board | A, WPA2 AP/client DHCP/NAT to SPI | Implemented; hardware acceptance pending |
| bridge-board | B, upstream Wi-Fi station, SPI peripheral, IPv4 NAPT | Implemented; hardware acceptance pending |
| spi-test-host | Temporary A image, SPI controller and network tests | Implemented; Wi-Fi never initialized |
| target-device | C6 Wi-Fi LED target C, outbound polling to UI PC | Implemented and built; third-board hardware acceptance pending |
| common | Shared portable protocol and ESP-NETIF/SPI driver | Used by B and test host |

Station and bridge use ESP-IDF **6.1** and support ESP32-S3 (Rev A PCB/default)
or ESP32-C6 (development boards with the separate
[C6 pinout](../hardware/docs/README.md#esp32-c6-development-board-wiring)).
The test-host image still targets ESP32-S3. Board C now uses ESP-IDF 6.1 and
ESP32-C6; it joins the station AP as an ordinary Wi-Fi client and uses the
[PC UI service](../web/README.md#control-the-third-c6).
See each project's README for commands. Station and bridge can also use a
[two-wire UART test link](../docs/uart-link.md) instead of SPI. SSIDs and
passwords belong in git-ignored sdkconfig, never tracked defaults. Generated
binaries also contain configured credentials and must not be published.

The [architecture](../docs/architecture.md) explains the intentional change
from command-only isolation to outbound IPv4 routing. The SPI test host remains
a separate diagnostic image without Wi-Fi; flash station-board onto A to use clients.
