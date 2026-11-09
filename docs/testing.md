# Testing

## Local software checks

Run the required suite from the repository root with Python 3.9+, Node 18+
and a C compiler with ASan/UBSan:

```sh
bash ci/run.sh
```

It runs service and browser-client tests, the pinned firmware parser and web
asset checks. Also run the gateway protocol/policy/UART tests (not included
in that service-focused CI script). Individual entrypoints are:

```sh
python3 tools/test/run_host_tests.py
python3 tools/test/test_device_server.py
python3 web/scripts/build.py
```

The host protocol runner builds the actual shared protocol/policy with a C
compiler supporting C11, warnings as errors,
AddressSanitizer and UndefinedBehaviorSanitizer. It checks the standard CRC
vector, corruption at every wire byte, malformed frames with recomputed CRCs,
MTU/EtherTypes, handshake, duplicate/old sessions, exact timeout boundary,
network-order status, subnet overlap, every readiness combination, source
spoofing, truncated L4 headers, direct-route bypass policy, multiple client
addresses and bounded UART encoding/overflow/resynchronization.

`test_device_server.py` runs eighteen tests against the real PC HTTP service on a
temporary loopback port, using a simulated Board C. It checks browser sessions,
command confirmation, timeouts, invalid messages, heartbeat expiry, concurrent
commands and restart recovery. It needs permission to bind a local port, but
does not change Wi-Fi settings or require physical boards.

`build.py` checks the frontend's forbidden APIs/external URLs and 60 KiB
gzipped budget. The PC service serves `web/src/` directly; generated assets
are not a deployment prerequisite.

Board C's parser runner verifies managed cJSON from an ESP-IDF build when
available; otherwise it downloads and hash-verifies the pinned archive. Run:

```sh
python3 tools/test/run_target_tests.py
```

If the Postman CLI is installed on PATH, also run:

```sh
python3 tools/test/run_device_api_tests.py
```

This optional harness runs the saved collection against the real local service
and a simulated device: 12 requests / 23 assertions. It supplies temporary
credentials, removes its environment file and disables result uploads. No
Postman login or shared workspace is required. See [test tooling](../tools/test/README.md).

LeakSanitizer cannot run under ptrace-based sandboxes. In that environment use
`ASAN_OPTIONS=detect_leaks=0 python3 tools/test/run_host_tests.py`; address/UB
checks still run. The tested portable codec/policy itself uses no heap.
The same `ASAN_OPTIONS=detect_leaks=0` workaround applies to `run_target_tests.py`.

Build firmware with ESP-IDF 6.1. Production `station-board` and `bridge-board`
support S3 or C6; `spi-test-host` supports S3 only; `target-device` supports
S3 or C6 (configure the verified carrier LED pin for S3). Choose the matching `idf.py set-target esp32s3` or
`idf.py set-target esp32c6` on initial configuration. Set-target resets
sdkconfig, so configure credentials afterward. See the [firmware index](../firmware/README.md).

## Recorded C6 home-network portal bench — 2026-11-09

One C6 previously running the red bridge role was temporarily flashed with
`target-device`. The Linux laptop hosted the existing PC service on its
home-network IPv4 address, held static for the active Wi-Fi session. The saved
laptop Wi-Fi profile remains DHCP; reconnecting or rebooting restores that
profile. This bench bypasses A/B and does not change the gateway demo's
`192.168.78.10:8000` endpoint.

On the separate home IoT network, the C6 associated and obtained DHCP. The
laptop received four of four ping replies, but device-initiated HTTP polls
to the laptop timed out. This demonstrates an unusable application path in
that arrangement, not proof of which router or firewall rule caused it.

On the home main network, WPA3 authentication initially failed repeatedly.
Explicitly selecting both SAE password-element methods, as the bridge already
does, allowed association, DHCP and real device polling. Wi-Fi disconnects now
log their reason code without logging credentials.

Software validation passed separately: `bash ci/run.sh` (22 service tests,
eight browser-client tests, sanitized target parser and web asset checks),
and ESP-IDF 6.1 target builds for C6 and S3. The S3 build used nonempty dummy
Wi-Fi settings and GPIO48 for compilation only; no S3 was flashed in this run.

| Physical C6 / browser check | Observation |
|---|---|
| Login and heartbeat | Browser sign-in succeeded; target became reachable and controls enabled |
| Output commands | Browser ON/OFF commands received matching device confirmations and status updates |
| Green output | Operator visually confirmed the controllable RGB LED was green after ON; optical OFF was not separately confirmed |
| Missing heartbeats | Pausing the C6 for more than eight seconds marked the target unavailable and disabled controls |
| Device restart | Restarted C6 rejoined Wi-Fi and restored fresh reports without reflashing |
| Service restart | Old sessions returned 401, browser required login again, and the C6 resumed polling |
| Events | Browser showed authentication, availability and confirmed-command entries; Allowed filtering worked |

At the end of the application test, the C6 was reachable with green output
and a running PC service. Private credentials, generated firmware and detailed local logs are
not committed. A/B forwarding, gateway isolation, the final Windows setup and
the third-board configuration remain separate acceptance work.

### Shutdown and school configuration restoration — 2026-11-09

After the bench test, LED OFF was acknowledged, the PC service was stopped,
and the laptop's saved DHCP settings were reapplied. Board C's private
configuration was restored to IoT-Gateway and the real demo endpoint
`192.168.78.10:8000`, with the passphrase matching Board A. The connected C6
was rebuilt and reflashed; flash hashes verified and its startup log confirmed
that server endpoint. It was then left in programming mode with the output
off, so its application and Wi-Fi remain stopped until reset or a power cycle.

The bridge's private configuration now selects the school SSID, OWE and an empty
passphrase. The saved laptop school profile reports OWE, so firmware now has
an explicit Enhanced Open option. That mode requires protected management
frames and rejects associations that are not OWE; personal WPA2/WPA3 remains
the default and still requires a passphrase.

Separate software validation passed: `bash ci/run.sh`, the sanitized gateway
host tests, the C6 target build, a C6 bridge OWE build and an isolated S3 bridge
personal-mode build with nonempty dummy credentials. No home PC endpoint is
present in the restored target image or tracked configuration defaults.
The prepared bridge image was not flashed onto the repurposed IoT-device C6.
School-network association, DHCP, DNS, internet access and any campus admission
requirements remain unverified until the actual bridge is tested at school.

### Three-C6 deployment — 2026-11-09

With all three C6 boards connected, chip identification confirmed C6 revision
0.0 on each port before flashing. The existing station kept its role, the
new board's blink demonstration was replaced with bridge firmware to match
the physical jumper pair, and the earlier target retained its role. All three ESP-IDF 6.1
builds passed; all bootloader, partition-table and application flash hashes
verified. Startup logs confirmed blue station, red bridge and the target
polling the real demo endpoint. Private USB identifiers and raw logs are not
committed.

The bridge associated to the school uplink using WPA3-OWE with protected
management frames and received an IPv4 lease. The IoT target associated to
the station AP and received a client lease. These are physical radio/DHCP
observations, separate from software validation.

Both gateway startup logs selected UART TX=6, RX=7 at 921600 baud. After the
role assignment matched the physical jumper pair, both gates opened. The
bridge's upstream DNS/HTTP probes and the station's DNS/HTTP probes through
UART passed. The target needs no signal jumpers.

The Linux laptop joined the station AP at the configured fixed UI address.
Its internet HTTP check passed, the real portal accepted login and received
fresh target reports, and a browser ON command received a matching device
acknowledgement. Full three-board restart/soak testing and gateway isolation
were not established by this check. A later SPI/isolation change is separate
from these UART observations.

## Merge integration checks — 2026-11-08

The PR #6 integration preserves main's events/theme/live-monitoring behavior
and S3 support alongside the A/B router and fixed-IP PC service. The browser
API is 0.3.2; the device wire protocol remains 0.1.0. The PC serves the UI/API
at **192.168.78.10:8000**; no UI is served by an ESP32.

| Check | Result |
|---|---|
| Required `bash ci/run.sh` | PASS; 18 real-service HTTP tests, 8 browser-client tests, sanitized parser and frontend asset checks |
| Shared gateway protocol/policy/UART and target parser | PASS with ASan/UBSan; leak detection disabled |
| Browser API evaluation suite | PASS; 19/19 scripts against a temporary mock |
| Saved Postman collection | PASS; 12 requests / 23 assertions against the real local service and device actor |
| JavaScript and legacy packer syntax | PASS |
| Web asset size | PASS; 13,348 / 61,440 bytes gzipped (21.7%) |
| Board C C6 and S3 firmware builds | PASS; isolated ESP-IDF 6.1 builds using compile-only settings |
| Gateway firmware builds | PASS; C6 station/bridge with UART and nonempty compile-only credentials, S3 SPI test host |
| Documentation integrity | PASS; 188 local links/anchors across 44 documents, no conflict markers or whitespace errors |

The later main update at `b90cd41` adds distinct reported-command failures,
reserved device worker capacity and expanded CI tests; those checks were rerun
before the follow-up push. Firmware sources did not change after the five builds.

Physical hardware tests were not repeated during conflict resolution. S3 bench
results and C6 UART router results are separate dated records, not proof of
full gateway isolation or Windows fixed-IP acceptance.

## Recorded local software validation — 2026-11-08

These results preceded integration of main; use the merge checks above for
the combined frontend and service.

| Check | Result |
|---|---|
| Shared gateway codec/session/policy/UART tests | PASS with ASan/UBSan; leak detection disabled |
| Board C report encoder/response parser with managed cJSON | PASS with ASan/UBSan; leak detection disabled |
| PC device service HTTP tests | PASS; 10 tests against the real service with a simulated device |
| Saved Postman API collection | PASS; 12 requests / 23 assertions, zero failures, exit code 0; real local service with a simulated device |
| Web asset build and constraints | PASS; 7,840 / 61,440 bytes gzipped (12.8%) |

These are workstation software checks. Firmware builds and physical tests
were not repeated for this documentation update; the dated records below
and [Board C validation record](../firmware/target-device/README.md#behavior-and-verification)
remain separate evidence.

## Recorded C6 UART validation — 2026-11-05

Both C6 gateway images were flashed and their writes verified. Hardware checks
confirmed AP/DHCP, UART sessions, upstream association, DNS/HTTP from both
boards, phone internet access, blue/red identity LEDs and peer-restart recovery.
Final UART uses compact envelope 0.2.0 at 921600 baud. The fixed UI .10 address
is excluded from DHCP .100–.199; four Wi-Fi slots are configured. Host tests
cover address-plan conflicts and outside TCP initiation versus allowed replies.

Simultaneous radio clients, Windows static-IP/UI-firewall setup, a packet-level
outside inbound attempt and an extended traffic soak remain unverified. See
[UART details](uart-link.md) and [client setup and boundaries](client-network.md).

## Recorded software validation — 2026-10-27

| Check | Result |
|---|---|
| ESP-IDF 6.1, ESP32-S3, B default configuration | PASS; empty credentials leave the uplink disabled |
| B with nonempty dummy build credentials | PASS; Wi-Fi code compiled/linked, no association attempted |
| A test host, default configuration | PASS |
| A test host with TCP/UDP echo configured | PASS; concurrent worker code compiled/linked |
| Production A, default configuration | PASS; empty AP password leaves Wi-Fi disabled |
| Production A with nonempty dummy AP password | PASS; AP/DHCP/NAPT code compiled/linked, no radio test performed |
| Portable C tests with AddressSanitizer/UndefinedBehaviorSanitizer | PASS; leak detection disabled for sandbox compatibility |
| Production A portable policy tests | PASS; private address validation, bridge status transitions, DNS selection, offline DHCP/ping, spoofing and inbound bypass |
| Two-endpoint recovery simulation | PASS with dropped transactions, independent/simultaneous resets and prolonged cable loss |
| Python echo server, simultaneous loopback TCP/UDP, 1472 bytes each | PASS on workstation; does not test SPI/NAT |

All six firmware configurations built without compiler warnings. Generated
configuration and binaries remain under ignored paths. The feature builds use
dummy credentials and a documentation-only echo address; they are compile
checks, not provisioned deployment images. At that validation point, no board
was flashed or physically tested. See [maintenance](firmware-maintenance.md) for component ownership and
the development workflow.

For an existing checkout with a mismatched build cache, follow
[A's setup guide](../firmware/station-board/README.md#configure-and-build)
before configuring credentials and flashing. Ignored local configurations
and build caches are workstation-specific, not fresh-checkout defaults.

## Hardware acceptance — pending

Basic C6 UART connectivity and peer-restart checks passed on 2026-11-05, as
recorded above. Full acceptance, including SPI electrical behavior and soak
testing, remains pending. Record firmware revision, board/module variants,
router/security mode, selected transport and clock/baud rate, date, results
and log locations when running these checks. A compile or unit-test pass does
not count as physical acceptance. The SPI pin row below is for S3/Rev A;
use the [C6 pinout](../hardware/docs/README.md#esp32-c6-development-board-wiring)
when testing SPI on C6.

| Test | Expected result |
|---|---|
| Continuity and logic analyzer | GPIO12/11/13/10/4 on both ends; shared GND, mode 0, 4 MHz, CS lengths exactly 12800 clocks; HS high only when DMA ready |
| Empty configuration | B boots into SPI diagnostic-only mode with configuration-error flag |
| Wrong password / unavailable SSID | No forwarding; bounded 1–30 s retry backoff; no credential disclosure |
| WPA2 and WPA3 personal | Association and DHCP reported separately; normal traffic after handshake |
| DHCP absent | 30 s watchdog reconnects; gate stays closed |
| Subnet overlap (/24 and broader upstream mask) | overlap flag set; forwarding remains closed |
| DNS and HTTPS | Test host resolves and authenticates HTTPS over SPI; radio remains off |
| TLS verification failure | Invalid hostname/expired/untrusted server fails, never falls back to insecure TLS |
| TCP/UDP echo | Configure lab echo server; both report PASS with exact payload contents |
| Simultaneous traffic | Enable echo tests: TCP/UDP workers run concurrently with HTTPS; no deadlock. Add sustained streams with a hardware host/harness for saturation |
| MTU | TCP transfer with 1460-byte segments, unfragmented UDP up to 1472 bytes works; oversized frames and fragmented IPv4 are rejected |
| Router outage | Gate closes, queues invalidated, NAT cleared; retries recover automatically |
| SPI unplug / A reset / B reset | Gate closes by 2 s timeout or immediately on observed new session; old frames do not reopen a pairing |
| DHCP address change | Session rotates, existing connections are discarded; new DNS/address reported |
| Corruption / short and long transactions / bad version / duplicate sequence | Rejected counter rises; no packet reaches upstream; later valid handshake recovers |
| Queue saturation | Drop counter rises; no unbounded memory growth, reset or watchdog failure |
| 30-minute soak | Repeat tests and concurrent transfers; no resets, stalls, sustained free-heap decline or buffer ownership errors |

### Unsolicited inbound test

On a controlled upstream PC, first attempt TCP/UDP to B's upstream address
without an outbound mapping: nothing should reach A. Also add a temporary
route for the SPI /30 via B's upstream address, and attempt direct TCP/UDP/ICMP
to A's SPI address. B's input hook must reject that bypass. Observe with a
logic analyzer or instrumented host/capture, then remove the temporary route.
After an outbound echo connection, only matching reply traffic should return.
Repeat during loss/recovery and after resetting either board.

## Web and Board C acceptance — pending

For UI development without hardware, run `python3 web/mock/server.py` and use
its [failure injection procedures](../web/mock/README.md#failure-injection).
It simulates the target; it cannot establish real LED state or reproduce
delivered-but-unconfirmed commands.

For the physical target, use `web/device_server.py` with Board C and the
fixed-address PC. Follow [Board C's acceptance steps](../firmware/target-device/README.md#behavior-and-verification):
confirm physical green/off output, availability after disconnect/reconnect,
device restart and service restart. Browser login and command validation are
implemented in the PC service. Device authentication, TLS and per-client
network policy remain unimplemented; WPA2 admission does not establish
compliance with the original security requirements.

## Production Board A acceptance — partial C6 UART validation

Flash `station-board` on A and `bridge-board` on B. The separate SPI test-host
image does not host Wi-Fi. Use client devices on A's SSID and an echo server on
the controlled upstream LAN. Record both serial logs and client network settings.

| Test | Expected result |
|---|---|
| Empty/short AP passphrase | No AP or client forwarding; configuration error logged without credentials |
| Wrong client password | Association fails; no DHCP lease issued |
| Correct password | WPA2/AES association; never open/WEP/TKIP; A never joins another Wi-Fi AP |
| DHCP while B is offline | Client gets .100-.199/24 and gateway .1; no usable DNS proxy exists before B supplies DNS (SDK may advertise gateway fallback); gateway ping works |
| First B readiness | DNS options arrive; clients briefly reassociate and obtain current DNS settings |
| DHCP renewal/DNS change | Gateway/mask/options remain correct; a DNS-only change on B propagates and triggers reassociation |
| DNS/HTTPS/TCP/UDP/ICMP | Client resolves names, verifies HTTPS certificates, completes echo tests and pings through both NAT layers |
| NAT addressing | SPI capture shows A's SPI IP as source, not client IP; upstream capture shows B's Wi-Fi IP |
| Multiple clients | Up to configured maximum work simultaneously; the next association is rejected without disturbing existing clients |
| Router outage/B reset/SPI unplug | A remains an AP with DHCP; forwarding closes, NAT clears, then new connections recover after fresh B readiness |
| A restart | Clients reassociate, obtain leases and reconnect; no previous SPI session/NAT state survives |
| AP subnet = SPI subnet | Invalid AP configuration rejected before Wi-Fi starts |
| AP subnet overlaps upstream /24 or broader prefix | A logs overlap and forwards nothing until configuration is corrected |
| Unsolicited inbound/direct route | Upstream connections without mappings fail; injected SPI packets addressed directly to AP clients or gateway are rejected |
| Malformed/off-subnet source | Client IPv4 spoofing, invalid lengths and fragments are dropped before NAPT parsing |
| 30-minute simultaneous transfer soak | No resets, stalls or sustained heap decline; A logs internal free/minimum heap every minute |

For a Linux client, inspect its Wi-Fi IPv4 address, default route and DNS after
joining. Test a LAN echo service, `ping` a permitted LAN destination, and
`curl --ipv4 --head https://example.com` without disabling certificate checks.
Capture traffic on the upstream test PC to verify the translated source.
Repeat after disconnecting SPI, restarting either board and changing upstream
DNS. Verify the DHCP client actually refreshes its DNS after reassociation;
clients that cache old options may need a manual Wi-Fi reconnect.

The firmware does not isolate associated clients at layer 2. Do not report
peer-client isolation or application authorization as a passing test.

For multiple clients, Windows UI static addressing, and outside-initiated
connection checks, also follow [the client-network procedure](client-network.md).
