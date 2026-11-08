# IoT Security Gateway

> **S3 live-test branch:** the login/control/events UI can use the real-device
> Python service and ESP32-S3 target. Start with [the live-test guide](docs/s3-live-test.md).
> The original architecture and scaffolding notes below describe the main-line
> proposal; this application test does not establish gateway isolation.

**CyberGuards, CSE 484.** A low-cost IoT security gateway built from three
ESP32-S3-DevKitC-1 boards. It protects an existing insecure IoT device without
changing that device.

Users reach the device through one HTTPS interface on the gateway. Every request
is authenticated and checked against an allow-list before it crosses a wired
SPI link to a second board. That board sends the request on to the device over
a private Wi-Fi network.

> **Status:** scaffolding only. The directory layout, contracts, and dev tooling
> are in place, but none of the features are built yet. See
> [CONTRIBUTING.md](CONTRIBUTING.md) before starting work.

## Topology

```
  [legitimate users and attackers on the normal Wi-Fi network]
            |  HTTPS, port 443, the only exposed service
  +---------+------------------------------+
  | BOARD A  "station board"               |  joins existing Wi-Fi
  |   HTTPS server, authentication,        |  SPI controller
  |   command allow-list, rate limiting,   |
  |   event log, and the WEB INTERFACE     |
  +---------+------------------------------+
            |  wired SPI link: SCLK, MOSI, MISO, CS, plus a handshake
            |  line, <= 10 MHz. THIS IS THE ONLY PATH between the
            |  two networks. No IP forwarding, no bridging, no NAT.
  +---------+------------------------------+
  | BOARD B  "bridge board"                |  hosts a private Wi-Fi AP
  |   SPI peripheral, frame validation,    |  SPI peripheral
  |   fixed-target HTTP client             |
  +---------+------------------------------+
            |  private Wi-Fi, password withheld from ordinary users
  +---------+------------------------------+
  | BOARD C  "target device"               |  deliberately insecure
  |   hardcoded credentials, no auth,      |
  |   plain HTTP, controls an LED          |
  +----------------------------------------+
```

**Why this is secure.** Each board has its own radio, and the boards share no
network stack. That means no software path on a single device can carry a
request from the user network to the target. A request must be authorized on
Board A and then re-issued across the wire by Board B. See
[docs/architecture.md](docs/architecture.md).

## Quick start

Clone the repo and create a branch (naming rules are in
[CONTRIBUTING.md](CONTRIBUTING.md#branching)):

```sh
git clone git@github.com:Tony-Computer-Science/IoT-Security-Gateway.git
cd IoT-Security-Gateway
git switch -c web/my-change        # or station/..., bridge/..., hw/...
```

**Web interface**, no hardware needed (Python 3 standard library only):

```sh
python3 web/mock/server.py         # serves web/src and a mock API on one port
python3 web/scripts/build.py       # minify + gzip into web/dist/, checks 60 KB budget
```

**Firmware** ([ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)):

```sh
cd firmware/station-board          # or bridge-board, target-device
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

## Repository map

| Path | What it is |
|---|---|
| [docs/](docs/) | Architecture, requirements R1–R7, REST API contract, SPI protocol, test guide |
| [firmware/station-board/](firmware/station-board/) | Board A: HTTPS, auth, policy, event log, SPI controller, serves the web UI |
| [firmware/bridge-board/](firmware/bridge-board/) | Board B: SPI peripheral, private AP, fixed-target HTTP client |
| [firmware/target-device/](firmware/target-device/) | Board C: the **intentionally vulnerable** device under protection |
| [web/](web/) | The browser interface served by Board A, plus its mock server and build scripts |
| [hardware/](hardware/) | SPI link wiring, pinout, KiCad schematic and PCB |
| [tools/](tools/) | Evaluation and attack scripts for the test plan |
| [.github/](.github/) | CODEOWNERS (inactive for now), PR template, issue templates |

## Team and ownership (provisional)

Ownership decides who **reviews** changes in an area. It does not limit who may
work there. Assignments will change as the project goes on; the current
version is always in [.github/CODEOWNERS](.github/CODEOWNERS) and in each
directory's README.

| Member | Current area |
|---|---|
| Antonio (Tony) Goncalves, project lead | `firmware/station-board`: Wi-Fi, HTTPS, certificates, authentication, sessions (`net_station`, `auth`) |
| Lal Za Lian | `firmware/station-board`: allow-list, input validation, rate limiting, event log (`policy`, `eventlog`) |
| Zachariah Grande | `firmware/bridge-board`, `firmware/target-device`, `spi_host` (shared) |
| Lalit Dhakal | `web/` |
| Carter Cattadoris | `hardware/`, `spi_host` (shared) |

## Requirements

The project is graded against requirements **R1–R7**, which are recorded
verbatim in [docs/requirements.md](docs/requirements.md). Reference them by
number in issues, commits, and PRs.
