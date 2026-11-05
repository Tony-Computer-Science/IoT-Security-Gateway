# IoT Security Gateway

**CyberGuards, CSE 484.** Two ESP32 gateway boards connect devices on a
private WPA2 access point to an existing Wi-Fi network through a wired link
(SPI by default, with an optional two-wire UART test mode).

```text
Devices -> Board A: WPA2 AP -> SPI/UART -> Board B: Wi-Fi station -> Router -> Internet
```

The board names remain **station-board (A)** and **bridge-board (B)** for
repository compatibility. These names do not describe their new Wi-Fi modes:
A is the access point; B is the upstream station.

## Current status

- Board B implements personal WPA2/WPA3 uplink, DHCP/reconnection, a bounded
  Ethernet-over-SPI/UART transport, IPv4 routing/NAPT, and restart isolation.
- `firmware/spi-test-host` is a separate Board A test image. Its radio stays
  off; DNS/HTTPS and configurable TCP/UDP echo tests use SPI only.
- Board A implements an AP-only WPA2/AES network, DHCP, DNS propagation and
  client NAT onto SPI. Application policy and the management UI remain future work.
- Board C is an ESP32-C6 Wi-Fi LED device that contacts a PC at
  **192.168.78.10:8000**. The PC serves the existing website and confirms LED
  commands against device acknowledgements. See [third-board setup](firmware/target-device/README.md).
  The firmware build and local API tests pass; third-board hardware testing is pending.
- ESP32-C6 UART hardware checks verified AP/DHCP, phone internet access and
  peer-restart recovery; see [UART results](docs/uart-link.md). SPI hardware,
  simultaneous clients and extended soak checks remain on the
  [acceptance checklist](docs/testing.md).
- Four Wi-Fi clients are supported by default, with .10 excluded from DHCP
  for a fixed UI PC. See [Windows setup and outbound-only routing](docs/client-network.md).

## Build and use

Use **ESP-IDF 6.1**, ESP32-S3 or ESP32-C6, at least 4 MB flash; PSRAM is not
required. The Rev A PCB and default target remain S3. C6 development boards
use the separate [C6 wiring](hardware/docs/README.md#esp32-c6-development-board-wiring)
and `idf.py set-target esp32c6` in each station/bridge project.
See [Board A instructions](firmware/station-board/README.md),
[Board B instructions](firmware/bridge-board/README.md) and
[test-host instructions](firmware/spi-test-host/README.md) for configuration,
flashing, wiring, and tests. Do not commit generated `sdkconfig` or binaries:
all Wi-Fi firmware binaries contain their configured credentials.

For the third C6, use [Board C instructions](firmware/target-device/README.md).
On the Windows UI PC, run `python web/device_server.py --host 192.168.78.10 --port 8000`
from this repository, then open `http://192.168.78.10:8000/` and use the
operator login printed by the service.

```sh
python3 tools/test/run_host_tests.py
```

## Design and requirements

This is an intentional change from the original fixed-target command gateway.
Normal internet routing conflicts with the original prohibition on packet
forwarding and with the original command allow-list. The original graded
[R1–R7 text](docs/requirements.md) is preserved verbatim; this milestone does
**not** claim compliance with that design. The differences and remaining
security work are explained in [architecture](docs/architecture.md).

Key references: [SPI contract 0.2.0](docs/spi-protocol.md),
[firmware maintenance](docs/firmware-maintenance.md),
[hardware pinout](hardware/docs/README.md), [contribution rules](CONTRIBUTING.md).
The existing KiCad layout is not fabricated; some silkscreen labels describe
the previous Wi-Fi roles. Rev A PCB wiring is unchanged; C6 boards use their
separate documented pinout.

For a demo with two signal jumpers plus common ground, station and bridge can
use the optional [UART test link](docs/uart-link.md). SPI remains the default.
