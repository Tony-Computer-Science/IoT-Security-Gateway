# IoT Security Gateway

**CyberGuards, CSE 484.** Two ESP32 gateway boards connect devices on a
private WPA2 access point to an existing Wi-Fi network through a wired link
(SPI by default, with an optional two-wire UART test mode).

```text
Devices -> Board A: WPA2 AP -> SPI/UART -> Board B: Wi-Fi station -> Router -> Internet
```

The board names remain **station-board (A)** and **bridge-board (B)** for
repository compatibility. These names do not describe their new Wi-Fi modes:
A is the access point; B is the upstream station. A third ESP32-C6 or ESP32-S3 (Board C)
joins A's AP and controls an RGB LED through a separate PC web service.

## Repository guide

| Path | Contents |
|---|---|
| [firmware/](firmware/README.md) | Gateway A/B images, Board C LED target, S3 SPI test host and shared components |
| [web/](web/README.md) | Browser interface, real-device PC service and standalone development mock |
| [hardware/](hardware/README.md) | S3 Rev A PCB and separate C6 SPI/UART wiring |
| [docs/](docs/README.md) | Architecture, API/link contracts, client setup, maintenance and testing |
| [tools/test/](tools/test/README.md) | Host protocol tests, HTTP tests, optional Postman checks and echo server |

For local UI work without boards, run `python3 web/mock/server.py` and open
the printed URL. The mock simulates the target; use the real-device service
in the setup below for physical LED control.

## Setting Up

The website and its API are a **separate service running outside the ESP32
boards**. For simplicity, this demo runs that service on a local computer
instead of a dedicated server: **the computer acts as the server**. It joins
**IoT-Gateway** and serves the UI/API at **http://192.168.78.10:8000/**.
Board C has that server IPv4 address and port **compiled into its firmware**
from menuconfig (`TARGET_UI_HOST` / `TARGET_UI_PORT`); it does not discover
the server automatically. Board C contacts it over Wi-Fi; station and bridge
provide networking. No ESP32 hosts the website or browser API.

Before the first run, build/configure and flash each board using the
[station](firmware/station-board/README.md),
[bridge](firmware/bridge-board/README.md) and
[IoT device](firmware/target-device/README.md) instructions. Bridge needs the
upstream Wi-Fi credentials; Board C needs the separate station AP password.
Keep passwords in private configuration, not this README.

1. **Power on station and bridge.** Keep their configured wired link connected.
   For the current two-wire UART demo, connect station GPIO6 TX to bridge GPIO7
   RX, station GPIO7 RX to bridge GPIO6 TX, and GND to GND. Both must use UART
   at 921600 baud; see [wiring and configuration](docs/uart-link.md).
   Each board also needs USB power. Check the controllable RGB LEDs against
   the table below. Their identity colors do not prove Wi-Fi or internet access.
2. **Join the station network from a phone and check internet access.** In
   the phone's Wi-Fi settings, join **IoT-Gateway** using the station AP
   password. Temporarily turn off mobile data so the test uses the gateway.
   Open `http://example.com` and confirm the page loads; do not rely only on
   the phone's connection label. If it fails, check that the phone joined
   **IoT-Gateway** and that bridge joined the intended upstream network
   (the configured school network, using passwordless OWE) with
   a valid IP address. Use the troubleshooting
   table below before proceeding with the internet test.
3. **Connect the computer that will host the web service.** Join that computer
   to **IoT-Gateway** too. On the Windows demo PC, set Wi-Fi IPv4 to
   **192.168.78.10**, subnet mask **255.255.255.0**, gateway **192.168.78.1**,
   and the upstream DNS supplied by bridge. Follow the
   [Windows fixed-IP and firewall steps](docs/client-network.md#windows-ui-computer).
   Check `ipconfig` shows .10 on the Wi-Fi adapter. Only this computer should
   use .10; the phone and IoT device obtain addresses automatically.
4. **Start the web service on that computer.** With Python 3.9 or later,
   open a terminal in this repository's root and run:

   ```powershell
   python web/device_server.py --host 192.168.78.10 --port 8000
   ```

   Leave the terminal open. Allow TCP 8000 from **192.168.78.0/24** using the
   scoped firewall rule in the Windows guide. On the PC or the phone, open
   `http://192.168.78.10:8000/`. Sign in as **operator** with the password
   printed by the service. This login password is separate from the Wi-Fi
   password. Use `device_server.py` for hardware control; the mock server
   only simulates a device. Until Board C connects, unavailable device status
   and disabled command buttons are expected.
5. **Power on the third C6, the IoT device.** Its firmware must have the
   **IoT-Gateway** SSID/passphrase, service address **192.168.78.10**, port
   **8000**, and device ID **iot-c6**. It joins Wi-Fi automatically and polls
   the service; it needs no signal jumpers to the other boards. Wait for
   reachable status in the website, then click **Turn on**: its controllable
   RGB LED should become **green** after device confirmation. Click **Turn
   off** and verify it goes dark. Off at startup is expected, not a connection
   failure. Station remains blue and bridge remains red.

| Board | Expected controllable RGB LED |
|---|---|
| Station / A | **Blue**, identifying the station firmware |
| Bridge / B | **Red**, identifying the bridge firmware |
| IoT device / C | **Off at startup**, **green after Turn on**, off after Turn off |

Look at the controllable RGB LED, not the separate power light, which may
stay green. These defaults apply to the matching C6 development boards with
the RGB LED on GPIO8. The phone, service PC and IoT device use three of the
station AP's four default client slots.

If a check fails:

| Symptom | What to check |
|---|---|
| Wrong board color or IoT-Gateway is missing | Confirm power, board labels and the correct firmware image. Check AP configuration and startup logs; a power light alone does not identify the firmware. |
| Phone joins but a webpage cannot load | Confirm mobile data is off, the phone joined IoT-Gateway and DHCP gave it 192.168.78.100–199 with gateway .1. Check bridge's upstream SSID/password and IP, and test that upstream network's internet separately. Check crossed UART wires, common ground and matching UART mode/baud. After bridge is online, reconnect the phone to refresh DHCP/DNS. |
| Service fails to bind to 192.168.78.10 | Confirm the service PC joined IoT-Gateway and .10 is assigned to its Wi-Fi adapter before launch. Check another program is not already using port 8000. |
| Website opens on the PC but not on the phone | Confirm both joined IoT-Gateway, the URL includes .10:8000, and the PC firewall permits TCP 8000 from 192.168.78.0/24. Do not disable the firewall. |
| Website opens but the device stays unavailable | Confirm Board C is flashed/configured for IoT-Gateway with the AP password, .10:8000 and matching device ID. Keep the real device service running. Inspect Board C's Wi-Fi/poll logs and the PC firewall. The LED can correctly be off while connected. |
| A command has an unknown result | Wait for a fresh device status report before retrying, then compare the reported state with the physical LED. Check service/device logs if confirmation continues to fail. |

Local website and LED control use the station AP and can work without
upstream internet; the phone's internet test checks a separate routing path.
No port forwarding or public exposure is needed for this local demo. A
dedicated server can host the separate service later, but Board C's configured
service IPv4 address/port and outbound network reachability must match that
deployment. The current device channel is lab-only plain HTTP; see the
[device API and its limitations](docs/device-api.md).

## Current status

- Board B implements personal WPA2/WPA3 or passwordless OWE uplink,
  DHCP/reconnection, a bounded Ethernet-over-SPI/UART transport, IPv4
  routing/NAPT, and restart isolation.
- `firmware/spi-test-host` is a separate Board A test image. Its radio stays
  off; DNS/HTTPS and configurable TCP/UDP echo tests use SPI only.
- Board A implements an AP-only WPA2/AES network, DHCP, DNS propagation and
  client NAT onto SPI. Per-device network policy remains future work; the
  website/API runs as the separate service described above.
- Board C is an ESP32-C6 Wi-Fi LED device that contacts a PC at
  **192.168.78.10:8000**. The PC serves the existing website and confirms LED
  commands against device acknowledgements. See [third-board setup](firmware/target-device/README.md).
  S3 physical LED/heartbeat/restart results are recorded in
  [bench evidence](docs/s3-test-evidence.md); C6 fixed-IP Windows acceptance is pending.
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
python3 tools/test/test_device_server.py
python3 web/scripts/build.py
```

These local checks need Python 3.9+; protocol tests also need a C compiler
with AddressSanitizer/UndefinedBehaviorSanitizer. Board C parser tests require
its managed cJSON dependency, and the optional collection check requires the
Postman CLI. See [all test commands and prerequisites](docs/testing.md#local-software-checks).

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
