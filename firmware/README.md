# Firmware

| Project | Role | Status |
|---|---|---|
| station-board | A, WPA2 AP/client DHCP/NAT to SPI | Implemented; hardware acceptance pending |
| bridge-board | B, upstream Wi-Fi station, SPI peripheral, IPv4 NAPT | Implemented; hardware acceptance pending |
| spi-test-host | Temporary A image, SPI controller and network tests | Implemented; Wi-Fi never initialized |
| target-device | Legacy intentionally insecure LED target C | Unchanged scaffold; not part of routing |
| common | Shared portable protocol and ESP-NETIF/SPI driver | Used by B and test host |

Station, bridge and test-host use ESP-IDF **6.1** and ESP32-S3. The older target
scaffold was documented for IDF 5.x and has not been migrated or validated in
this milestone. See each project's README for commands. SSIDs and
passwords belong in git-ignored sdkconfig, never tracked defaults. Generated
binaries also contain configured credentials and must not be published.

The [architecture](../docs/architecture.md) explains the intentional change
from command-only isolation to outbound IPv4 routing. The SPI test host remains
a separate diagnostic image without Wi-Fi; flash station-board onto A to use clients.
