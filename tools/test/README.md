# Gateway tests

- `run_host_tests.py`: compiles/runs the shared C protocol, sessions and packet
  policy with address/undefined-behavior sanitizers. Requires Python 3 and a C
  compiler; standard library only. Run from any directory.
- `test_gateway_protocol.c`: malformed-wire, CRC, session/reset, addressing and
  pre-NAT firewall regression cases.
- `test_gateway_access.c`: production A's private subnet configuration, B status
  validation, DNS selection, offline DHCP/ping, source spoofing and NAT bypass.
- `echo_server.py --bind <lab-PC-IP> --port 9000`: TCP/UDP echo endpoint for
  the separate ESP32-S3 SPI host; at most eight TCP clients, bounded buffers.

## Board C and PC service

Build `firmware/target-device` with ESP-IDF 6.1 first to obtain its pinned
managed cJSON dependency, then run from the repository root:

```sh
ASAN_OPTIONS=detect_leaks=0 python3 tools/test/run_target_tests.py
python3 tools/test/test_device_server.py
python3 tools/test/run_device_api_tests.py
python3 web/scripts/build.py
```

- `run_target_tests.py` compiles the exact firmware report encoder/response
  parser and pinned cJSON with ASan/UBSan. It covers truncated/oversized JSON,
  nesting, escapes, duplicates, unknown actions, invalid IDs and report bounds.
  Requires a C compiler and the downloaded managed dependency.
- `test_device_server.py` runs ten stdlib tests against the real PC HTTP
  service with a protocol-level device actor. It checks login/logout, LED
  on/off confirmation, missing/wrong acknowledgements, device/PC restarts,
  stale heartbeats, command contention, malformed requests and event logging.
  It binds only a temporary loopback port and never changes Wi-Fi.
- `run_device_api_tests.py` runs the saved
  [Postman collection](device-api.postman_collection.json) against the real
  local service and actor: 12 requests / 23 assertions. Requires `postman`
  on PATH. The harness supplies a random temporary login password, cleans up
  its private environment file, and disables run-result uploads. No Postman
  account or shared workspace is required for this local run.

All four checks passed on 2026-11-05. HTTP evidence uses a stand-in device;
it does not test the firmware radio, RGB LED, Windows firewall or physical
command path. Use [Board C acceptance steps](../../firmware/target-device/README.md#behavior-and-verification)
for those. No passwords or generated run environments belong in Git.

Run network tests only against your own controlled lab. No credentials are
stored in these tools. See [hardware acceptance](../../docs/testing.md) and
[test-host setup](../../firmware/spi-test-host/README.md). These tests cover
the router and PC application separately; they do not implement all four
original command-gateway evaluation categories or assert compliance with the
original R1–R7 specification.
