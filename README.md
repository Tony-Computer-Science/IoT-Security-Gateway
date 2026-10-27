# IoT Security Gateway

**CyberGuards, CSE 484.** Two ESP32-S3 gateway boards connect devices on a
private WPA2 access point to an existing Wi-Fi network through a wired SPI link.

```text
Devices -> Board A: WPA2 AP -> SPI -> Board B: Wi-Fi station -> Router -> Internet
           implemented              implemented, hardware validation pending
```

The board names remain **station-board (A)** and **bridge-board (B)** for
repository compatibility. These names do not describe their new Wi-Fi modes:
A is the access point; B is the upstream station.

## Current status

- Board B implements personal WPA2/WPA3 uplink, DHCP/reconnection, a bounded
  Ethernet-over-SPI transport, IPv4 routing/NAPT, and restart isolation.
- `firmware/spi-test-host` is a separate Board A test image. Its radio stays
  off; DNS/HTTPS and configurable TCP/UDP echo tests use SPI only.
- Board A implements an AP-only WPA2/AES network, DHCP, DNS propagation and
  client NAT onto SPI. Application policy and the management UI remain future work.
- The existing web interface/mock and intentionally vulnerable target firmware
  belong to the original LED-command demonstration and are unchanged.
- No hardware tests have been run for the router implementation. See the
  [acceptance checklist](docs/testing.md).

## Build and use

Use **ESP-IDF 6.1**, ESP32-S3, at least 4 MB flash; PSRAM is not required.
See [Board A instructions](firmware/station-board/README.md),
[Board B instructions](firmware/bridge-board/README.md) and
[test-host instructions](firmware/spi-test-host/README.md) for configuration,
flashing, wiring, and tests. Do not commit generated `sdkconfig` or binaries:
both router binaries contain their configured Wi-Fi credentials.

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
the previous Wi-Fi roles. Wiring is unchanged.
