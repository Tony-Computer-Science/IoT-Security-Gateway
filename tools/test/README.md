# Gateway tests

- `run_host_tests.py`: compiles/runs the shared C protocol, sessions and packet
  policy with address/undefined-behavior sanitizers. Requires Python 3 and a C
  compiler with ASan/UBSan; standard library only. Run from any directory.
- `test_gateway_protocol.c`: malformed-wire, CRC, session/reset, addressing and
  pre-NAT firewall regression cases.
- `test_gateway_access.c`: production A's private subnet configuration, B status
  validation, DNS selection, offline DHCP/ping, source spoofing and NAT bypass.
- `test_gateway_uart.c`: compact COBS envelope round trips, overflow,
  truncation, stream resynchronization and inner CRC checks. Included in
  `run_host_tests.py` alongside the protocol/access tests.
- `echo_server.py --bind <lab-PC-IP> --port 9000`: TCP/UDP echo endpoint for
  the separate ESP32-S3 SPI host; at most eight TCP clients, bounded buffers.

Python services and checks use Python 3.9+. If LeakSanitizer is unavailable
in a restricted environment, prefix either C runner with
`ASAN_OPTIONS=detect_leaks=0`; ASan/UBSan checks remain enabled.

## Board C and PC service

The required `bash ci/run.sh` also runs eight browser-client tests with
Node 18+, the target parser and asset validation alongside HTTP and mutual-TLS suites. OpenSSL is needed to generate temporary
private test identities.

Run from the repository root. The target parser runner uses and verifies
managed cJSON from an ESP-IDF build when available; otherwise it downloads
the pinned archive and verifies its SHA-256 hashes:

```sh
ASAN_OPTIONS=detect_leaks=0 python3 tools/test/run_target_tests.py
python3 tools/test/test_device_server.py
python3 tools/test/test_device_isolation.py
python3 tools/test/run_device_api_tests.py
python3 web/scripts/build.py
```

- `run_target_tests.py` compiles the exact firmware report encoder/response
  parser and pinned cJSON with ASan/UBSan. It also checks the exact target IPv4 policy: PC application traffic, peer
  application denial, local ping, DHCP and malformed/fragmented packets. It
  covers truncated/oversized JSON,
  nesting, escapes, duplicates, unknown actions, invalid IDs and report bounds.
  Requires a C compiler; a first run without managed cJSON needs network access.
- `test_device_server.py` runs twenty-two stdlib tests against the real PC HTTP
  service with a protocol-level device actor. It checks login/logout, LED
  on/off confirmation, missing/wrong acknowledgements, device/PC restarts,
  stale heartbeats, command contention, malformed requests and event logging.
  It binds only a temporary loopback port and never changes Wi-Fi.
- `test_device_isolation.py` runs ten real TLS/admission checks with temporary
  private CAs and EC identities: valid client, missing/untrusted/wrong pinned
  certificate, no plaintext fallback, PC-only UI, bounded idle TLS handshakes
  and loopback-only software actors. These do not use a real ESP32.
- `run_device_api_tests.py` runs the saved
  [Postman collection](device-api.postman_collection.json) against the real
  local service and actor: 12 requests / 24 assertions. Requires `postman`
  on PATH. The harness supplies a random temporary login password, cleans up
  its private environment file, and disables run-result uploads. No Postman
  account or shared workspace is required for this local run.

See the [dated validation results](../../docs/testing.md#recorded-local-software-validation--2026-11-08)
for the latest rerun. HTTP evidence uses a stand-in device;
it does not test the firmware radio, RGB LED, Windows firewall or physical
command path. Use [Board C acceptance steps](../../firmware/target-device/README.md#behavior-and-verification)
for those. No passwords or generated run environments belong in Git.

Run network tests only against your own controlled lab. No credentials are
stored in these tools. See [hardware acceptance](../../docs/testing.md) and
[test-host setup](../../firmware/spi-test-host/README.md). These tests cover
the router and PC application separately; they do not implement all four
original command-gateway evaluation categories or assert compliance with the
original R1–R7 specification.

## Browser API evaluation suite

## Test categories

The four categories below come from the evaluation plan in the first submittal
(section V). Rename them here and in the directory names together if the plan
is renumbered.

| # | Category | Requirements | Scripts |
|---|---|---|---|
| 1 | Authorized operation | R6 | `login_success.py`, `status_reads.py`, `command_applies.py` |
| 2 | Unauthorized access | R2, R7 | `no_token_refused.py`, `bad_password_refused.py`, `forged_token_refused.py`, `token_replay_after_logout.py`, `lockout_after_failures.py`, `restart_invalidates_sessions.py` |
| 3 | Policy and isolation | R1, R3, R4 | `invalid_json_rejected.py`, `unknown_path_rejected.py`, `wrong_method_rejected.py`, `oversized_body_rejected.py`, `invalid_action_rejected.py`, `event_detail_has_no_secret.py` |
| 4 | Rate limits and recovery | R4, R6, R7 | `command_rate_limit.py`, `link_down_refuses_command.py`, `target_unreachable_refuses_command.py`, `recovery_after_link_restore.py` |

Directory layout:

```
tools/test/
├── common.py                  shared client and PASS/FAIL reporter
├── run_all.py                 runs every case script
├── 1-authorized-operation/
├── 2-unauthorized-access/
├── 3-policy-and-isolation/
└── 4-rate-limits-and-recovery/
```

## Running

Against the mock server:

```sh
MOCK_PASSWORD=lab-password python3 web/mock/server.py &
GATEWAY_USER=operator GATEWAY_PASSWORD=lab-password python3 tools/test/run_all.py
```

The full evaluation suite needs the standalone mock: restart/link-failure
cases use `/__mock/` routes, which the real PC service deliberately disables.
A passing mock run does not establish physical gateway isolation. For real
service checks use `test_device_server.py` or the saved API harness above.
Individual applicable evaluation scripts can take the real PC base URL:

```sh
GATEWAY_USER=... GATEWAY_PASSWORD=... python3 tools/test/1-authorized-operation/login_success.py http://192.168.78.10:8000
```

A single case can be run on its own, for example:

```sh
python3 tools/test/2-unauthorized-access/lockout_after_failures.py
```

## Conventions

- One script per test case, named after what it checks
  (e.g. `login_lockout.py`).
- Each script prints PASS/FAIL per check and exits non-zero on any failure.
- Credentials come from environment variables (`GATEWAY_USER`,
  `GATEWAY_PASSWORD`), never from arguments that end up in shell history, and
  never from committed files (R2).
- Record results (date, firmware commit, outcome) in the test plan, not in
  this directory.
- A dropped connection is reported as status `0` and counts as a failure,
  because the contract requires a JSON error body for every rejected request.
