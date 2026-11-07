# ESP32-S3 live device test

This test connects the existing login, control and events pages to a real
outbound-polling target. It combines the frontend from main at `d8046bb` with
Board C and the Python device service from `codex/spi-wifi-router` at `653a152`.
It does not include that branch's A/B router firmware. A working test here
establishes browser/service/target integration, not gateway isolation or the
A/B wired link.

## Requirements

- Python 3.9+ for the service and ESP-IDF 6.1 for the firmware.
- ESP32-S3 N16R8 (16 MB flash, 8 MB PSRAM) or the existing C6 target.
- A verified carrier-board WS2812 LED GPIO. The module name alone is insufficient.
  S3 builds deliberately fail until a pin is configured. Do not flash a C6 image
  onto an S3, or guess a pin from another carrier.
- A private 2.4 GHz WPA2/WPA3-compatible network under your control. The target
  and API service must be mutually reachable. Campus Wi-Fi/client isolation
  is unsuitable. The device channel is unauthenticated HTTP by design.

## Start the service

From the repository root:

```sh
python3 web/device_server.py --host <SERVICE_LAN_IP> --port 8000 --device-id iot-s3
```

Open `http://<SERVICE_LAN_IP>:8000/`. Sign in as `operator` with the randomly
generated password printed by the process. `UI_PASSWORD` can supply a private
local password. Only bind to the intended test interface. The mock server is
still available for offline development, but does not control an ESP.

For development on a remote machine, bind the service to `127.0.0.1:8765`
and forward it from the computer connected to the ESP's Wi-Fi:

```sh
ssh -N -o ExitOnForwardFailure=yes \
  -L 127.0.0.1:8765:127.0.0.1:8765 \
  -L <LOCAL_WIFI_IP>:8000:127.0.0.1:8765 <DEVELOPMENT_HOST>
```

The browser uses `http://127.0.0.1:8765/`; the ESP uses `<LOCAL_WIFI_IP>:8000`.
The SSH tunnel must stay running. Confirm local firewall access to the bound
Wi-Fi address. This forwarding carries the application test traffic only.

## Build and flash

Activate ESP-IDF 6.1, then:

```sh
cd firmware/target-device
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <USB_PORT> flash monitor
```

Under **Board C IoT device**, set the private Wi-Fi SSID/password, service IPv4
address/port, `iot-s3` device ID, and verified WS2812 LED pin. S3-specific defaults
select 16 MB flash and USB Serial/JTAG console. PSRAM is not required. For the
existing C6 board use `set-target esp32c6` and its verified GPIO8 carrier.

Back up existing firmware before the first flash. Keep sdkconfig, flash backups,
and credential-bearing binaries private and outside version control. When USB
and compilation are on different computers, transfer the build's bootloader,
partition table, app and flashing arguments privately; use the offsets generated
for that exact target. The original board image is the rollback artifact.

## Acceptance

1. Before first contact: output is Unknown, device unavailable, commands disabled.
2. After boot: the page shows a fresh device report and enables controls.
3. Turn on/off: observe the physical LED and compare it with the page. A successful
   acknowledgement confirms the driver reported success, not optical feedback.
4. Events: confirm login, device availability and command entries; exercise filters.
5. Disconnect/reconnect: after eight seconds without a heartbeat, the channel
   becomes unavailable and controls disable. Reconnection restores availability.
6. Restart the service: the browser must require login again.
7. Lost acknowledgement: the page must say the result is unconfirmed, retain the
   last reported output and avoid automatic command retries.

The live view labels PC uptime as **API service uptime** and labels the
heartbeat-based signal **Device channel**. Its explanatory note explicitly
states that A/B link and network isolation are not measured.

## Repeatable software checks

```sh
bash ci/run.sh
# After an ESP-IDF build installs managed cJSON:
python3 tools/test/run_target_tests.py
```

The Python tests use a software device actor over real HTTP. Keep that test
result separate from physical LED/radio acceptance. The PR workflow is gated by repository variable `ENABLE_HOSTED_TESTS=true`.
The owner must first verify private Actions allowance: billing and Actions
permissions were unavailable to the current account. Until enabled, the job is
skipped; a skipped job is not a passing validation. Firmware builds and physical
acceptance remain separate from the portable hosted check.

See [test evidence](s3-test-evidence.md) for the current validation and open items.
