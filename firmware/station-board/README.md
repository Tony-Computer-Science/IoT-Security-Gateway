# Board A: WPA2 access point and SPI router

ESP32-S3 or ESP32-C6, ESP-IDF **6.1**, at least 4 MB flash; no PSRAM required.
Devices join
A's WPA2 personal access point. A provides DHCP, advertises B's DNS servers and
NATs client traffic onto its SPI address. B then NATs that traffic onto its
upstream Wi-Fi address. HTTPS stays encrypted between client and destination.

```text
Devices -> A: WPA2 AP / DHCP / NAT -> SPI -> B: NAT / Wi-Fi station -> LAN / Internet
```

On C6-WROOM-1 DevKitC-1 boards, the onboard GPIO8 RGB LED shows **blue**
to identify this firmware role. This is a fixed identity color, not a network
readiness indicator. Configure or disable it under **Board identity LED**.

## Components

| Component | Responsibility |
|---|---|
| `access_point` | WPA2/AES configuration, private /24, DHCP, DNS option refresh and association events |
| `station_router` | B status validation, subnet checks, NAT lifecycle and IPv4 hooks |
| `../common/gateway_link` | SPI controller, virtual Ethernet and bounded transport queues |
| `../common/gateway_protocol` | Shared codec/session rules and portable routing policy |
| `main` | Initialization and component wiring only |

The old `net_station` and `spi_host` placeholders were replaced. The legacy
`auth`, `policy` and `eventlog` scaffolds remain available for later work but
are not started. The website and command API belong to a separate service,
run on a local computer for the demo; see [Setting Up](../../README.md#setting-up).
The `www` partition is retained, unmounted, from the former onboard-hosting
design and is not used by this setup. This image replaces `spi-test-host`
on A during real client tests; B uses the same SPI 0.2.0 protocol.

## Configure and build

Activate the installed environment, then configure A independently of B:

```sh
. /home/tonyg/.espressif/tools/activate_idf_v6.1.sh
cd /home/tonyg/Documents/College/IoT-Security-Gateway/firmware/station-board
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Use the board's actual serial port. The example selects ESP32-C6; use
`idf.py set-target esp32s3` for the Rev A PCB/S3 boards. A fresh checkout still
defaults to S3. Run `set-target` only for initial setup or a target change:
it resets sdkconfig, including Wi-Fi settings. Back up private settings first,
then restore them through menuconfig. Subsequent builds only need `idf.py build`.

If sdkconfig already selects C6 but an old build cache selects S3, preserve
sdkconfig and move the old `build/` into ignored `local-backups/`, then run
`idf.py build`. Editing sdkconfig.defaults alone does not migrate an existing
configuration or cached build. The workstation's C6 migration backups are in
`local-backups/`; they include private configuration and must not be published.
See [Board B's instructions](../bridge-board/README.md#configure-and-build)
for the environment and flashing troubleshooting.

Under **Component config → Station board WPA2 access point**, configure:

- SSID (default `IoT-Gateway`, 1–32 bytes) and your own 8–63 byte passphrase.
  The password defaults to empty: A will not start an open AP or forward
  packets until valid credentials are supplied. Neither passwords nor packet
  contents are logged. Settings and binaries contain credentials; keep them private.
- AP gateway, default **192.168.78.1/24**. Use an RFC1918 private address ending
  in `.1`. DHCP assigns `.100` through `.199`, with five-minute leases by default.
  `.10` is excluded from DHCP for a manually configured UI computer.
- Channel 1–11 (default 6), maximum associated clients 1–10 (default 4), and
  lease duration. The configured AP channel is independent of B's Wi-Fi channel.

Under **Gateway board link**, keep the controller enabled and match B's address
settings: A **192.168.77.2/30**, B **192.168.77.1/30** by default. A, SPI and B's
upstream LAN must all be distinct subnets. A refuses forwarding on overlap,
even if B itself can reach the upstream network. The SPI interface is A's
explicit default route. Wi-Fi runs in AP-only mode.

Use the [target-specific pinout](../../hardware/docs/README.md): C6 defaults
are SCLK=6, MOSI=7, MISO=2, CS=18, HS=19; S3 retains SCLK=12, MOSI=11,
MISO=13, CS=10, HS=4. Wire corresponding signals and common GND between boards.
Pins are configurable under **Gateway board link** and logged at startup.
On C6, GPIO12/13 are native USB pins and must be left free for USB flashing
and monitoring. Mode 0, MSB-first, 4 MHz; the
[wire contract](../../docs/spi-protocol.md) is unchanged.

## Multiple clients and a fixed UI computer address

The default maximum is **four simultaneous devices**, including the UI PC.
Use **192.168.78.10/24**, gateway **192.168.78.1**, for the UI computer and
configure that address on the computer. DHCP is restricted to .100–.199 so
it cannot allocate .10. These host ranges and the maximum client count are
configurable; startup rejects conflicting or invalid address plans.

Clients on the AP can reach the UI PC directly. New connections from the
upstream network are blocked; matching responses to client-initiated traffic
remain usable. Both pre-NAT filters reject incoming TCP SYN without ACK, and
port mapping/IPv6 are excluded from this gateway configuration. This does
not isolate clients admitted to the same AP from one another.

See [Windows setup and verification](../../docs/client-network.md).

## Two-wire UART test option

For tests with only two signal jumpers and an existing common ground, enable
**Component config → Gateway board link → Use two-wire UART instead of SPI**
on **both** boards. Keep Board A endpoint enabled on station and disabled on
bridge. Default settings: UART1, 921600 baud, 8-N-1, no hardware flow control.

- Station GPIO6 (TX) → bridge GPIO7 (RX).
- Bridge GPIO6 (TX) → station GPIO7 (RX).
- Keep GND connected; power and flash through each board's USB cable.

Rebuild and flash both images. This mode does not use SCLK, CS or HS wires.
The existing /30 addresses, sessions, status, packet checks, DHCP and NAT
remain in use. Throughput is much lower than SPI; use it for basic connectivity
and recovery checks. It does not validate SPI wiring or performance. See the
[UART envelope and test procedure](../../docs/uart-link.md).

SPI remains the tracked default. To return to it, disable the UART option on
both endpoints and restore the target-specific SPI wiring.

## Recovery and DNS

A waits for B's valid session, forwarding status and matching SPI addresses
before enabling client NAT. Loss or restart closes forwarding and clears A's
NAT mappings before recovery. The AP and DHCP remain available during ordinary
SPI/upstream outages; old network connections may need to reconnect.

DHCP advertises usable IPv4 DNS servers learned from B. With only one, the
SDK DNS slots repeat that primary address because IDF 6.1 rejects clearing a
slot with 0.0.0.0. There is no hardcoded external DNS service or local DNS
proxy. Before the first usable B
status, DNS offering is disabled; IDF 6.1 may still advertise the AP gateway
as its fallback DNS address, which is not a DNS proxy. On first discovery or
any DNS address change, A restarts DHCP and briefly deauthenticates associated
devices to prompt fresh leases. Devices normally reassociate automatically; a client that retains old
settings may require reconnecting Wi-Fi. Upstream loss alone retains the last
advertised DNS options and does not disconnect clients. B now refreshes the
advertised DNS values even when DHCP changes DNS without changing its IP.

## Limits and validation

IPv4 TCP/UDP and ICMP echo; no IPv6, IP fragmentation, inbound port mapping,
enterprise Wi-Fi, captive portal or application filtering. SDK NAPT creates
TCP/UDP mappings for ordinary ephemeral source ports >=1024. Packet hooks
reject spoofed off-subnet client sources, direct SPI routes into the AP subnet,
and routed client access to SPI infrastructure addresses. They do not provide
per-client identity or Wi-Fi client isolation: associated clients can still
communicate at layer 2. WPA2 admission is not application command authorization.

Run `python3 tools/test/run_host_tests.py` from the repository root and build
A, B and the test-host image after shared code changes. See the
[maintenance guide](../../docs/firmware-maintenance.md) and
[hardware acceptance checklist](../../docs/testing.md). Compilation and host
policy tests pass. C6 UART hardware checks verified Wi-Fi, DHCP, phone internet
access and peer-restart recovery; see [recorded UART results](../../docs/uart-link.md).
Simultaneous clients, the separate Windows UI PC, an outside packet-level
connection attempt, SPI electrical behavior and a 30-minute soak remain
unverified; see [client-network checks](../../docs/client-network.md).
