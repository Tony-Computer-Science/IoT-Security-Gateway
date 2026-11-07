# S3 integration test evidence — 2026-11-07

## Scope and provenance

Frontend base: main `d8046bb19fa0c862fa9cf1e40bd42cdfa8b44ecf`.
Existing device/backend/parser code reused from router branch
`653a152f0ce86d6470fc5ba0658789aac00b7523`. This change adds S3 build support,
combines that backend with the latest login/control/events pages, and corrects
live monitoring labels, unknown initial state, heartbeat freshness and ambiguous
command messaging. A/B firmware is unchanged from main.

## Passed locally

- Thirteen real-HTTP tests: sessions, actual command round trip with a software
  actor, malformed input, one command in flight, restart/late-ack behavior,
  unknown initial output, stale heartbeat and recovery, event-only outage
  detection without duplicate expiry entries.
- Web asset validation: 12,472 bytes gzip, below the 61,440-byte budget.
- JavaScript syntax checks for app.js and api.js.
- ESP-IDF 6.1 release (`fff9895c82d744c7237be8847347bdd1b07c6643`): full S3 and C6
  builds. S3 image 0xcd520 bytes; C6 image 0xef1b0 bytes. Both include Wi-Fi/HTTP
  paths using dummy compile-only network settings. The S3 compile check used
  GPIO8 solely to compile the RMT driver: it is NOT the verified carrier pin,
  and neither image is approved for flashing onto the connected board.
- Exact firmware response parser compiled with strict warnings and
  UndefinedBehaviorSanitizer, against pinned cJSON 1.7.19~2. Tests cover valid
  commands, malformed JSON, truncation, duplicates, invalid actions and reports.
- Chrome UI with a software actor: bad login rejected with attempts remaining;
  valid login; on/off acknowledged; command events; Alert filtering; heartbeat
  expiry while Events remained active; service restart invalidated session;
  lost acknowledgement produced an explicit unconfirmed result.
- MacBook-to-mini loopback SSH forwarding returned HTTP 200 for the page.

## Hardware observations

USB identification only: ESP32-S3 revision 0.2, 16 MB quad flash and 8 MB PSRAM.
A full 16 MB original flash backup was copied to both operator machines with
matching SHA-256 checksums. No flash erase/write or security-fuse change occurred.
The backup is private and excluded from this repository.

## Pending or limited

- Exact carrier/WS2812 pin, private Wi-Fi configuration, S3 flash, physical LED,
  radio reliability and unplug/reconnect acceptance remain pending.
- Tony's A/B gateway and network isolation have not been tested by this setup.
- On this macOS host, AddressSanitizer hangs inside its allocator/shadow-memory
  initialization before test main. UBSan passed using
  `python3 tools/test/run_target_tests.py --sanitizers undefined`. ASan is not
  reported as passed; the default runner retains both sanitizers and has a
  30-second execution timeout.
- Hosted tests are gated by `ENABLE_HOSTED_TESTS=true` until the owner verifies
  private Actions allowance. Billing/Actions permission and branch-protection
  queries returned 404 to the current account; enforcement is not claimed.
- Area-owner review and shared-contract review are required before merge.

## Screenshots (software actor, not physical hardware)

- [Connected simulator](assets/s3-test/simulator-online.png): live-device labels
  and the most recent device report.
- [Disconnect event](assets/s3-test/simulator-disconnect-event.png): Alert filter
  and heartbeat expiry while the Events page remains active.
