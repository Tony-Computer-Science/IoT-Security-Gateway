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

ESP32-S3 revision 0.2, 16 MB quad flash and 8 MB PSRAM on an HW678 carrier.
A full 16 MB original flash backup was copied to both operator machines with
matching SHA-256 checksums before flashing. The backup is private and excluded
from this repository. No security-fuse changes were made.

The live S3 image built successfully with ESP-IDF 6.1, DIO/16 MB flash,
USB Serial/JTAG console and GPIO48. The app is 841,008 bytes. Bootloader,
partition table and application were transferred with matching SHA-256 hashes,
then flashed at the exact build-generated offsets; esptool verified written data.
Wi-Fi credentials and the firmware containing them remain private.

Physical bench acceptance passed:

- The target joined the private Wi-Fi and sent reports through the MacBook's
  LAN-bound SSH forward to the development service. No software actor was running.
- Browser login succeeded; Control showed UP/REACHABLE and fresh device reports.
- An authenticated API LED-on command was acknowledged by the physical ESP;
  Ivan visually confirmed the RGB LED glowed green, verifying GPIO48 on this unit.
- Clicking TURN OFF in the browser produced an acknowledgement and OFF status;
  Ivan visually confirmed the RGB LED turned off while the power light remained on.
- Events showed both confirmed commands. With Events open, holding the ESP in its
  ROM bootloader stopped reports. The eight-second heartbeat timeout generated an
  alert. Control then showed DOWN/UNREACHABLE and disabled both command buttons.
- A USB hard reset restarted the firmware. The target rejoined, fresh reports
  restored UP/REACHABLE and enabled controls, and Events recorded the restart and
  recovered channel. This is a controlled firmware-stop/restart test, not a USB
  unplug or extended radio reliability test.

The selected GPIO48 mapping was corroborated by an
[HW-678 implementation](https://github.com/shantanuk47/ESP32_S3_N8R2_RGB)
for another memory variant; physical observation above verifies this tested unit.

## Pending or limited

- Extended radio reliability, physical USB unplug/replug and operation after
  laptop sleep or a LAN-address change remain untested.
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

## Screenshots (physical S3 target)

- [Connected S3](assets/s3-test/hardware-online.png): fresh reports and acknowledged
  ON state, paired with Ivan's visual confirmation of the green LED.
- [Heartbeat expiry](assets/s3-test/hardware-heartbeat-expired.png): Alert filter
  while the physical target firmware was stopped in its ROM bootloader.
- [Hardware events](assets/s3-test/hardware-events.png): confirmed on/off commands,
  heartbeat expiry, device restart and recovered channel.

## Monitoring enhancement validation — 2026-11-07

- Fifteen backend tests pass, including shared confirmation history, preservation
  across failed/pending commands and device restarts, service-reset clearing,
  mock parity, and exclusion of late unconfirmed acknowledgements.
- Browser JavaScript syntax and asset budget pass. Firmware is unchanged.
- Real S3/browser: device uptime differed from freshly restarted service uptime;
  ON acknowledgement populated command history. Stopping firmware marked device
  uptime offline and disabled commands. Restart reset ESP uptime and reported LED
  OFF while preserving the historical ON confirmation. A subsequent browser OFF
  command replaced the historical record with OFF.
- Browser-only status-request failure injection marked retained uptime and history
  stale and disabled controls; removing the failure restored live monitoring.
- Desktop and 390px mobile layouts were visually inspected.

Screenshots: [live monitoring](assets/s3-test/live-monitoring.png),
[offline report](assets/s3-test/live-monitoring-offline.png), and
[device restart with retained command history](assets/s3-test/live-monitoring-restart.png).
