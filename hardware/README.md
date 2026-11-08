# hardware/

The physical side of the gateway: the SPI or optional UART link between
Board A and Board B, S3/C6 pinouts, Board C's RGB LED, and the KiCad schematic
and PCB. The unfabricated Rev A PCB uses two ESP32-S3 modules. The current
UART demo uses C6 development boards; the separate Board C target is also C6.

## Current owner (provisional)

Carter Cattadoris. Any member may work here; the current owner reviews.

## Layout

| Path | Purpose |
|---|---|
| [kicad/](kicad/) | KiCad project: schematic and PCB for the inter-board link |
| [docs/](docs/) | Pinout table, wiring diagram, bring-up notes |

## The SPI link

- Signals: SCLK, MOSI, MISO, CS, plus one handshake line, plus a common ground.
- Clock: **4 MHz by default**; up to 10 MHz only after hardware validation.
- This link carries routed traffic between A's private AP and B's upstream
  Wi-Fi station (see [architecture](../docs/architecture.md)). The existing
  PCB labels describe the previous reversed Wi-Fi roles.
- The optional [UART test mode](../docs/uart-link.md) replaces SPI with
  crossed TX/RX and common ground. Use one configured transport at a time.
  Each board's USB connection is for power, flashing and diagnostics.
- Board C and the UI PC communicate locally over A's AP; their LED control
  traffic does not traverse the A/B link.

Pin assignments are recorded in [the wiring guide](docs/README.md). Rev A/S3
uses SCLK 12, MOSI 11, MISO 13, CS 10 and HS 4; C6 has a separate pinout.
Firmware defaults live in `firmware/common/gateway_link/Kconfig` and can be
changed in menuconfig. Match those settings to the actual wiring on both ends.

## Opening the KiCad project

Install [KiCad](https://www.kicad.org/) 10 (TODO: agree on a team version, since
newer KiCad files can't be opened by older versions). Open
`kicad/IoT-Gateway.kicad_pro`.

Per-user and generated files (`*.kicad_prl`, `*-backups/`, `fp-info-cache`) are
git-ignored.
