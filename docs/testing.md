# Testing

How to test each component. Sections marked TODO will be filled in as features
land.

## Web interface (`web/`)

No hardware needed.

1. **Mock server.** Start it and open the printed URL:
   ```sh
   python3 web/mock/server.py
   ```
   Use the failure-injection endpoints to exercise R6 and R7 without hardware.
   Every injection is documented with curl examples in
   [web/mock/README.md](../web/mock/README.md):
   - `link_ok: false` → commands must show the **link-down** state (503)
   - `target_reachable: false` → commands must show **target-unreachable** (502)
   - `latency_ms: 1500` → the UI must show **pending** while waiting
   - `force_lockout: true` → login must show **rate-limited** / locked out (429)
   - `/__mock/reset` → simulates a gateway restart: all sessions invalid (R2, R7)
2. **Build and budget check.**
   ```sh
   python3 web/scripts/build.py
   ```
   Exits non-zero if the gzipped total is over 60 KB, or if the minified
   output contains `localStorage`, `sessionStorage`, `WebSocket`, or an
   external URL.
3. **Offline check.** Open the browser's network panel with the page loaded.
   There must be no request to any host other than the page's own origin.
4. TODO: an automated UI test approach, if the team wants one. It must not add
   runtime dependencies.

## Firmware

Each project should build cleanly:

```sh
cd firmware/<board> && idf.py set-target esp32s3 && idf.py build
```

- TODO: Unity unit tests for `policy` (allow-list, validation, rate-limit
  arithmetic) and `eventlog` (ring buffer wraparound). These can run on target
  via ESP-IDF's `test_apps` pattern.
- TODO: host-side tests for the SPI frame encode/decode, if the frame code is
  kept free of ESP-IDF dependencies.

## Hardware

TODO: SPI link bring-up checklist (continuity, logic-analyzer capture at the
target clock, handshake timing). See [hardware/README.md](../hardware/README.md).

## End-to-end evaluation and attack tests

See [tools/test/README.md](../tools/test/README.md). These scripts run against
the real gateway or the mock server and are organized by the four test plan
categories.
