# Testing

## Local software checks

```sh
python3 tools/test/run_host_tests.py
```

This builds the actual shared protocol/policy with C11, warnings as errors,
AddressSanitizer and UndefinedBehaviorSanitizer. It checks the standard CRC
vector, corruption at every wire byte, malformed frames with recomputed CRCs,
MTU/EtherTypes, handshake, duplicate/old sessions, exact timeout boundary,
network-order status, subnet overlap, every readiness combination, source
spoofing, truncated L4 headers and direct-route bypass policy.

LeakSanitizer cannot run under ptrace-based sandboxes. In that environment use
`ASAN_OPTIONS=detect_leaks=0 python3 tools/test/run_host_tests.py`; address/UB
checks still run. The tested portable codec/policy itself uses no heap.

Build `firmware/station-board`, `firmware/bridge-board` and `firmware/spi-test-host` with ESP-IDF 6.1
and `idf.py set-target esp32s3` on initial configuration. Set-target resets
sdkconfig, so configure credentials afterward. See each project's README.

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
checks, not provisioned deployment images. No board was flashed or physically
tested. See [maintenance](firmware-maintenance.md) for component ownership and
the development workflow.

Production A was initially checked using separate ignored build/configuration
paths. The older normal `sdkconfig` and build cache still targeted ESP32 and
caused a normal build failure. Those files have since been backed up under
`firmware/station-board/local-backups/`, and the normal configuration migrated
to ESP32-S3 while preserving gateway options. See
[A's setup guide](../firmware/station-board/README.md) before configuring credentials
and flashing.

## Hardware acceptance — pending

No attached boards were available during implementation. Record firmware
revision, board/module variants, router/security mode, SPI clock, date, results
and log locations when running these checks. A compile or unit-test pass does
not count as physical acceptance.

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

## Legacy web/target tests

The existing `python3 web/mock/server.py` and `python3 web/scripts/build.py`
continue to support the original LED-command interface. They do not exercise
the new router. Application command authorization and per-device policy remain
future work; WPA2 admission does not establish compliance with those requirements.

## Production Board A acceptance — pending

Flash `station-board` on A and `bridge-board` on B. The separate SPI test-host
image does not host Wi-Fi. Use client devices on A's SSID and an echo server on
the controlled upstream LAN. Record both serial logs and client network settings.

| Test | Expected result |
|---|---|
| Empty/short AP passphrase | No AP or client forwarding; configuration error logged without credentials |
| Wrong client password | Association fails; no DHCP lease issued |
| Correct password | WPA2/AES association; never open/WEP/TKIP; A never joins another Wi-Fi AP |
| DHCP while B is offline | Client gets .2-.100/24 and gateway .1; DNS omitted before B has first supplied servers; gateway ping works |
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
