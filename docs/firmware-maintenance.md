# Firmware maintenance

Start with [architecture](architecture.md), then the short startup function in
[Board A main](../firmware/station-board/main/main.c) and
[Board B main](../firmware/bridge-board/main/main.c). Networking behavior lives
in components, not in startup. Shared components are compiled into A, B and
the separate A test application, avoiding multiple copies of the protocol.

## Where to make changes

| Concern | Source | Validation |
|---|---|---|
| Wire fields, CRC and Ethernet shape | `firmware/common/gateway_protocol/gateway_frame.c` | Host framing tests; update the protocol document and both endpoints |
| Pairing, sequence and timeout rules | `firmware/common/gateway_protocol/gateway_session.c` | Host session and two-endpoint recovery tests |
| Diagnostic wire payload | `firmware/common/gateway_protocol/gateway_status.c` | Status round-trip and byte-order tests |
| Address checks and pre-NAT IPv4 policy | `firmware/common/gateway_protocol/gateway_policy.c` | Policy tests; hardware inbound/overlap tests |
| AP address/status validation and client packet policy | `firmware/common/gateway_protocol/gateway_access.c` | Board A policy tests; DHCP/outage/inbound checks |
| WPA2 AP, DHCP and advertised DNS | `firmware/station-board/components/access_point/access_point.c` | A builds; client association, leases and DNS refresh |
| Client NAT and forwarding lifecycle | `firmware/station-board/components/station_router/station_router.c` | A builds; double NAT, simultaneous clients and outage recovery |
| UART COBS envelope and stream bounds | `firmware/common/gateway_protocol/gateway_uart.c` | Host UART tests; update both endpoints and the UART contract |
| SPI DMA/GPIO handshake, UART I/O, queues and virtual Ethernet | `firmware/common/gateway_link/gateway_link.c` | All three gateway builds; logic analyzer, reset and saturation tests |
| A/B identity LEDs and optional network probes | `firmware/common/board_led/`, `firmware/common/gateway_diagnostics/` | A/B builds; role colors and diagnostic logs on hardware |
| Association, DHCP and retry handling | `firmware/bridge-board/components/uplink_wifi/uplink_wifi.c` | B build; wrong credentials, DHCP and outage tests |
| NAT lifecycle and lwIP hooks | `firmware/bridge-board/components/bridge_router/bridge_router.c` | B build; forwarding, inbound and loss/recovery tests |
| Repeatable end-to-end checks | `firmware/spi-test-host/main/network_tests.c` | A test build; verified HTTPS and simultaneous echo tests |
| Board C Wi-Fi, polling and retry lifecycle | `firmware/target-device/main/main.c` | C6 target build; physical device/PC acceptance |
| Board C report encoder and response parser | `firmware/target-device/main/target_protocol.c` | Target parser tests with pinned managed cJSON |
| Board C RGB output | `firmware/target-device/main/target_led.c` | Target build; physical green/off and startup checks |
| PC service sessions and command confirmation | `web/device_server.py`, reused `web/mock/server.py` logic | Real-service HTTP tests and saved API collection; device contract |

Portable protocol files have no ESP-IDF, RTOS, heap or socket dependencies.
Keep decisions that can be tested on a host there; keep SDK calls and lifecycle
effects in their owning components. Public headers describe byte order, buffer
lifetime and call restrictions. Prefer named helpers for meaningful operations
and comments explaining constraints, rather than comments repeating each line.

## Concurrency and ownership

- The SPI task owns transaction descriptors and DMA buffers. Once queued to the
  peripheral driver, a buffer remains untouched until completion, even while a
  receive wait times out. ISR callbacks only raise/lower HS.
- `state_lock` protects pairing, gate, generation, status and counters. Never
  hold it across SDK calls, logging or queue operations. TX copies are tagged
  with their generation; old generations are dropped.
- RX uses a pool of eight packets. ESP-NETIF/lwIP owns a submitted buffer until
  `release_rx_buffer()`. Session reset must never reclaim those buffers. The
  pool, RTOS queues and DMA allocations have application lifetime.
- The default event loop serializes Wi-Fi actions. Timer callbacks post events
  without blocking. `status_lock` protects the published uplink snapshot; do
  not hold it while calling other components.
- One supervisor owns each endpoint's SPI netif lifecycle and NAPT. B enables
  NAT on SPI; production A enables NAT on its AP. A's AP event loop owns its
  running flag; the router supervisor alone changes DHCP DNS options after
  startup. DNS changes cause a DHCP restart and client reassociation, while
  upstream outages retain the AP and its last lease options.
- lwIP input/forwarding hooks run on `tcpip_thread` and check readiness again
  for queued packets. B compares its actual upstream interface with the lease;
  A checks B's latest status, address agreement and subnet separation. DHCP
  and local gateway ping are explicitly allowed on A while forwarding is off.

The bring-up order is **netif up → NAPT enabled → data gate open**. Teardown is
**gate closed → NAPT disabled/cleared → netif down**. This order matters:
disabling NAPT alone would permit ordinary routing, while disabling NAPT after
the netif is down would not clear its state correctly in this SDK.
On A, the AP normally stays up; teardown disconnects only SPI after clearing
AP NAT. If the AP itself stops unexpectedly, its NAT cleanup is deferred until
that interface is up again, and forwarding stays closed until cleanup succeeds.
A logs free/minimum internal heap once per minute for the hardware soak check.

A reset request closes the gate immediately and invalidates queued generations.
The SPI task rotates its session during maintenance. A previously prepared DMA
transaction may still finish, but its old pairing cannot inject new data.
Control messages continue while forwarding is unavailable. A fresh pairing and
upstream readiness are required to resume. IPv4 readiness means a lease exists;
it does not assert that the internet works.

Board C is a separate Wi-Fi client application and does not use the gateway
SPI/UART driver. Its single polling task owns the HTTP response buffer, last
command acknowledgement and LED state. The PC service admits one pending
command at a time using a condition variable on its state lock, waits up to
five seconds for confirmation, and limits HTTP handling to 16 concurrent
connections with six-second socket timeouts. Its heartbeat deadline is eight
seconds. See [device lifecycle and limits](device-api.md).

## Development checks

Run the portable tests from the repository root:

```sh
python3 tools/test/run_host_tests.py
```

Under a ptrace sandbox, use `ASAN_OPTIONS=detect_leaks=0`; address and undefined
behavior checks remain enabled. Build **all three gateway projects** after shared changes,
using [the installed ESP-IDF 6.1 environment](../firmware/bridge-board/README.md#configure-and-build).
Also build A/B with nonempty test credentials and the test host with echo configuration: empty
defaults can allow the compiler to discard optional paths.

After Board C changes, build `firmware/target-device` for C6 and run
`python3 tools/test/run_target_tests.py`. For service changes, run
`python3 tools/test/test_device_server.py` and, when Postman is installed,
`python3 tools/test/run_device_api_tests.py`. The target parser runner needs
the managed cJSON installed by the firmware build. See [the complete local
check procedure](testing.md#local-software-checks).

The root `.clang-format` defines the C style. Use clang-format 21 or newer on
only changed `.c`/`.h` files; do not format SDK sources or generated build trees.
On this workstation it is installed at
`~/.espressif/tools/esp-clang/esp-21.1.3_20260408/esp-clang/bin/clang-format`.

```sh
clang-format -i path/to/changed.c path/to/changed.h
git diff --check
```

When upgrading ESP-IDF, inspect custom-driver RX ownership, hook ordering
relative to NAPT, NAPT teardown, and SPI `trans_len` saturation before claiming
compatibility. The current peripheral reserves a four-byte guard beyond the
1600-byte wire frame to detect overlong transactions. IPv4 fragments are
rejected before NAPT because this SDK invokes NAPT before reassembly. Its ICMP
translation supports echo; TCP/UDP outbound mappings use source ports >=1024.
Keep these limitations visible in architecture and acceptance documentation.

Hardware results must be recorded separately from builds and host tests. Use
the [acceptance checklist](testing.md); do not infer electrical timing, NAT
correctness, throughput or heap stability from compilation.
