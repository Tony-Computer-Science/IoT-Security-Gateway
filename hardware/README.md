# hardware/

The physical side of the gateway: the wired SPI link between Board A and Board
B, the pinout of all three ESP32-S3-DevKitC-1 boards, and the KiCad schematic
and PCB.

## Current owner (provisional)

Carter Cattadoris. Any member may work here; the current owner reviews.

## Layout

| Path | Purpose |
|---|---|
| [kicad/](kicad/) | KiCad project: schematic and PCB for the inter-board link |
| [docs/](docs/) | Pinout table, wiring diagram, bring-up notes |

## The SPI link

- Signals: SCLK, MOSI, MISO, CS, plus one handshake line, plus a common ground.
- Clock: ≤ 10 MHz.
- This wire is the **only** path between the user-side network and the target
  network (see [docs/architecture.md](../docs/architecture.md)). No other
  connection between Board A and Board B (UART, shared USB hub to one host,
  etc.) may carry data in the final setup.

Pin assignments are recorded in [docs/README.md](docs/README.md) (set by the
PCB, Rev A: SCLK 12, MOSI 11, MISO 13, CS 10, HS 4 on both boards). Firmware
reads them from there.

## Opening the KiCad project

Install [KiCad](https://www.kicad.org/) 10. Open
`kicad/IoT-Gateway.kicad_pro`.

Per-user and generated files (`*.kicad_prl`, `*-backups/`, `fp-info-cache`) are
git-ignored.
