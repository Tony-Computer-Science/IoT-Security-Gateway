# hardware/docs: pinout, wiring, bring-up

The single source of truth for pin assignments. Firmware (`spi_host`,
`spi_device`, target LED) must use the pins recorded here.

## Current owner (provisional)

Carter Cattadoris. Any member may work here; the current owner reviews.
Changes to the pinout should also be reviewed by whoever owns the affected
firmware component.

## Two-wire UART test link

Enable **Gateway board link → Use two-wire UART instead of SPI** on both
station and bridge, rebuild and flash both. With common GND already connected,
only two signal jumpers are required:

| Station (A) | Bridge (B) |
|---|---|
| GPIO6 (TX) | GPIO7 (RX) |
| GPIO7 (RX) | GPIO6 (TX) |
| GND | GND (existing connection) |

These are GPIO numbers, not header positions. UART1 uses 921600 baud, 8-N-1
and no RTS/CTS. USB remains available for power, flashing and logs. Remove old
SPI signal wiring when using this mode; GPIO6/7 are reused by C6 SPI. Use short
jumpers. No CS, clock or handshake signal is used in UART mode. See the
[UART test guide](../../docs/uart-link.md). SPI remains the default for Rev A.

## C6 board identity LEDs

ESP32-C6-WROOM-1 DevKitC-1 uses an addressable RGB LED on GPIO8. Production
station firmware sets it blue and bridge sets it red at startup. These are
identity colors, not forwarding status. The GPIO is configurable under
**Board identity LED**; disable the option on hardware without this LED.
S3/Rev A defaults leave this option disabled. GPIO8 is not part of the
board-to-board link. See the
[Espressif DevKitC-1 guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html).

## ESP32-C6 development-board wiring

The Rev A PCB pinout below is for ESP32-S3. ESP32-C6 development boards need
separate wiring; they are not pin-compatible replacements on that PCB.
Station and bridge select these defaults when targeting `esp32c6`:

| Signal | Board A GPIO | Board B GPIO | Direction |
|---|---|---|---|
| SCLK | 6 | 6 | A → B |
| MOSI | 7 | 7 | A → B |
| MISO | 2 | 2 | B → A |
| CS | 18 | 18 | A → B |
| Handshake (HS) | 19 | 19 | B → A |
| GND | GND | GND | Common ground |

These pins are exposed on Espressif C6 DevKitC-1 and DevKitM-1 boards; verify
headers on other models. GPIO12/13 are C6 USB D−/D+ and must remain free for
native USB flashing/monitoring. C6 strapping pins are GPIO4, 5, 8, 9 and 15;
this link avoids them and the UART0 pins. See Espressif's
[C6 GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/peripherals/gpio.html)
and [DevKitM-1 pinout](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitm-1/user_guide.html).

Pin choices are configurable in **Component config → Gateway board link** on
each endpoint. Match signals, not header positions. Fit the same 10 kΩ CS
pull-up at B and HS pull-down at A described below. Startup logs print the
selected pins. Compilation does not validate board wiring or SPI timing.

## ESP32-S3 / Rev A PCB pinout

Defined by the PCB (KiCad project in [../kicad/](../kicad/), Rev A) and used
unchanged on the breadboard demo (PCB requirement HW-12): **Board A and Board B
use the same GPIO numbers.** Every pin below was checked against the ESP32-S3
strapping pins (GPIO0, 3, 45, 46) and the pins the WROOM-1 module reserves for
flash and PSRAM (GPIO26–32 always; GPIO35–37 too on octal-PSRAM R8 modules).
None of the link pins is a strapping or reserved pin.

### Board A (station, SPI controller) ↔ Board B (bridge, SPI peripheral)

| Signal | Board A GPIO | Board B GPIO | Direction | Notes |
|---|---|---|---|---|
| SCLK | 12 | 12 | A → B | FSPICLK (SPI2 IO_MUX pin). 33 Ω series at A (R10) |
| MOSI | 11 | 11 | A → B | FSPID (SPI2 IO_MUX pin). 33 Ω series at A (R11) |
| MISO | 13 | 13 | B → A | FSPIQ (SPI2 IO_MUX pin). 33 Ω series at B (R16) |
| CS | 10 | 10 | A → B | FSPICS0 (SPI2 IO_MUX pin). 33 Ω series at A (R12). 10 kΩ pull-up at B (R15): idles deselected while A boots |
| Handshake (HS) | 4 | 4 | B → A | Driven by B, input on A. Active high: B raises HS only when a DMA transaction is ready, even if it contains no data; A polls and starts the transfer (SPI contract 0.2.0). 33 Ω series at B (R17). 10 kΩ pull-down at A (R13): idles low while B boots, so A never sees a false "data ready" |
| GND | GND | GND | | Common ground required. On the PCB this is the shared ground plane |

Because SCLK, MOSI, MISO and CS sit on the SPI2 (FSPI) IO_MUX pins, firmware
can use SPI2 without going through the GPIO matrix. HS is a plain GPIO.

### Other pins used on both boards

| Function | GPIO / pin | Notes |
|---|---|---|
| USB D− / D+ | GPIO19 / GPIO20 | Native USB: flashing and serial console. Each board has its own USB-C port; USB data never crosses between A and B |
| BOOT | GPIO0 | Push button to GND (internal pull-up). Strapping pin, used for nothing else |
| EN (reset) | EN | Push button to GND, 10 kΩ pull-up + 1 µF RC power-up delay |
| UART0 TXD0 / RXD0 | GPIO43 / GPIO44 | Not used (console is native USB) |
| Strapping GPIO3, GPIO45, GPIO46 | — | Left unconnected |

All other GPIOs are unconnected on the PCB.

### Board C (target)

| Function | GPIO | Notes |
|---|---|---|
| C6 RGB LED data | GPIO8 (configurable) | Green=on, black=off on the matching C6 development board; Board C is not on the PCB. Confirm carrier-board wiring if using another model. |

Board C uses Wi-Fi to IoT-Gateway and needs no signal wiring to A or B.
See [Board C setup](../../firmware/target-device/README.md).

## Wiring

- **PCB (Rev A):** the SPI and HS lines run about 75 mm between the modules on
  the top layer over a solid ground plane, with 33 Ω source termination at the
  driving end. Routing is good to well above 10 MHz (it doesn't limit the link
  below 40 MHz).
- **Breadboard demo:** wire GPIO*n* on Board A to GPIO*n* on Board B for each
  row of the table above (10, 11, 12, 13, 4), plus GND. Fit the pull-up on CS at
  Board B and the pull-down on HS at Board A (10 kΩ each) so the idle states
  match the PCB.
- TODO: wiring diagram (put the image in `docs/assets/` or here), wire lengths,
  connector type.

## Bring-up checklist

TODO, suggested steps:

1. Continuity check of every link wire, including GND.
2. Logic-analyzer capture of SCLK/CS at the intended clock.
3. Loopback test at low clock, then step up to the target clock.
4. Handshake line timing.
5. Behavior when one board is reset or the cable is unplugged (R7).
