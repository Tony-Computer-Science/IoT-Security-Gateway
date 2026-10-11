# hardware/docs: pinout, wiring, bring-up

The single source of truth for pin assignments. Firmware (`spi_host`,
`spi_device`, target LED) must use the pins recorded here.

## Current owner (provisional)

Carter Cattadoris. Any member may work here; the current owner reviews.
Changes to the pinout should also be reviewed by whoever owns the affected
firmware component.

## Pinout

**Nothing is assigned yet.** Fill these in, then update `docs/spi-protocol.md`
§1 to match. Check each choice against the ESP32-S3 strapping pins and the
pins the DevKitC-1 module reserves for flash and PSRAM before committing it.

### Board A (station, SPI controller) ↔ Board B (bridge, SPI peripheral)

| Signal | Board A GPIO | Board B GPIO | Direction | Notes |
|---|---|---|---|---|
| SCLK | TODO | TODO | A → B | ≤ 10 MHz |
| MOSI | TODO | TODO | A → B | |
| MISO | TODO | TODO | B → A | |
| CS | TODO | TODO | A → B | |
| Handshake | TODO | TODO | TODO | Meaning defined in spi-protocol.md |
| GND | GND | GND | | Common ground required |

### Board C (target)

| Function | GPIO | Notes |
|---|---|---|
| LED | TODO | On-board RGB LED, or external LED + resistor? |

## Wiring

TODO: wiring diagram (put the image in `docs/assets/` or here), wire lengths,
connector type.

## Bring-up checklist

TODO, suggested steps:

1. Continuity check of every link wire, including GND.
2. Logic-analyzer capture of SCLK/CS at the intended clock.
3. Loopback test at low clock, then step up to the target clock.
4. Handshake line timing.
5. Behavior when one board is reset or the cable is unplugged (R7).
