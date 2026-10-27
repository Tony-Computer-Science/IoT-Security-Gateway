# Board A: WPA2 access point and SPI router

ESP32-S3, ESP-IDF **6.1**, at least 4 MB flash; no PSRAM required. Devices join
A's WPA2 personal access point. A provides DHCP, advertises B's DNS servers and
NATs client traffic onto its SPI address. B then NATs that traffic onto its
upstream Wi-Fi address. HTTPS stays encrypted between client and destination.

```text
Devices -> A: WPA2 AP / DHCP / NAT -> SPI -> B: NAT / Wi-Fi station -> LAN / Internet
```

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
are not started. No web server or command API is provided. The `www` partition
is retained, unmounted, for later UI work. This image replaces `spi-test-host`
on A during real client tests; B uses the same SPI 0.2.0 protocol.

## Configure and build

Activate the installed environment, then configure A independently of B:

```sh
. /home/tonyg/.espressif/tools/activate_idf_v6.1.sh
cd /home/tonyg/Documents/College/IoT-Security-Gateway/firmware/station-board
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Use the board's actual serial port. Run `set-target` only for initial setup or
migration from the old ESP32 configuration: it resets sdkconfig. Preserve any
existing settings you need before migrating. The normal local `sdkconfig` and
`build/` have now been migrated to ESP32-S3, with gateway settings preserved.
The previous configuration and build output are backed up under the ignored
files in `local-backups/`. No target reset is needed for subsequent builds.
The environment details also appear in
[Board B's instructions](../bridge-board/README.md#configure-and-build).

If another checkout reports an ESP32/ESP32-S3 target mismatch, both its old
`sdkconfig` and CMake build cache need migration. Back up any settings you need,
run `idf.py set-target esp32s3`, then restore your gateway options through
menuconfig. Changing `sdkconfig.defaults` alone does not migrate existing
configuration or cached build files.

Under **Component config → Station board WPA2 access point**, configure:

- SSID (default `IoT-Gateway`, 1–32 bytes) and your own 8–63 byte passphrase.
  The password defaults to empty: A will not start an open AP or forward
  packets until valid credentials are supplied. Neither passwords nor packet
  contents are logged. Settings and binaries contain credentials; keep them private.
- AP gateway, default **192.168.78.1/24**. Use an RFC1918 private address ending
  in `.1`. DHCP assigns `.2` through `.100`, with five-minute leases by default.
- Channel 1–11 (default 6), maximum associated clients 1–10 (default 4), and
  lease duration. The configured AP channel is independent of B's Wi-Fi channel.

Under **Gateway SPI link**, keep the controller enabled and match B's address
settings: A **192.168.77.2/30**, B **192.168.77.1/30** by default. A, SPI and B's
upstream LAN must all be distinct subnets. A refuses forwarding on overlap,
even if B itself can reach the upstream network. The SPI interface is A's
explicit default route. Wi-Fi runs in AP-only mode.

Wire GPIO12 SCLK, GPIO11 MOSI, GPIO13 MISO, GPIO10 CS, GPIO4 HS and common GND
between the boards. Mode 0, MSB-first, 4 MHz; see the
[wire contract](../../docs/spi-protocol.md). No protocol version change is needed.

## Recovery and DNS

A waits for B's valid session, forwarding status and matching SPI addresses
before enabling client NAT. Loss or restart closes forwarding and clears A's
NAT mappings before recovery. The AP and DHCP remain available during ordinary
SPI/upstream outages; old network connections may need to reconnect.

DHCP advertises up to two usable IPv4 DNS servers learned from B. There is no
hardcoded external DNS service or local DNS proxy. Before the first usable B
status, leases omit DNS. On first discovery or any DNS address change, A
restarts DHCP and briefly deauthenticates associated devices to prompt fresh
leases. Devices normally reassociate automatically; a client that retains old
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
policy tests pass; physical Wi-Fi, DHCP, double NAT, restart recovery and the
30-minute soak remain pending until boards are connected.
