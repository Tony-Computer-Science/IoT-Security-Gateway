# firmware/target-device: Board C

ESP-IDF **6.1**, **ESP32-C6**, 4 MB flash. Board C joins **IoT-Gateway** over
Wi-Fi and polls the UI computer at **192.168.78.10:8000**. It uses the existing
website's `led_on` / `led_off` commands. It needs USB power and no signal
jumpers to station or bridge.

This is an intentionally vulnerable lab target: its device channel is
unauthenticated plain HTTP. Use it only on the private lab AP. Device IDs
are public labels, not authentication; admitted AP peers can impersonate the
target. Wi-Fi credentials stay in ignored sdkconfig, never tracked source.

## Current owner (provisional)

Zachariah Grande. Any member may work here; the current owner reviews.

## UI computer

Configure the Windows PC's fixed address and scoped firewall rule using
[client-network.md](../../docs/client-network.md), then from the repository
root run with Python 3.9 or later:

```powershell
python web/device_server.py --host 192.168.78.10 --port 8000
```

Open `http://192.168.78.10:8000/`. Sign in as `operator` using the password
printed by the server. It is separate from the Wi-Fi password. Set the local
`UI_PASSWORD` environment variable before starting the server to keep a chosen
login password across runs. No Python packages are required.

The standalone mock server does not control hardware; use `device_server.py`.
Only one IoT target is supported by the current browser API, alongside other
ordinary Wi-Fi clients on the AP.

## Configure, build and flash

Activate your ESP-IDF 6.1 environment, then:

```sh
cd firmware/target-device
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py -p <THIRD_BOARD_PORT> flash monitor
```

Under **Board C IoT device**, set:

| Setting | Value |
|---|---|
| Station AP SSID | `IoT-Gateway` |
| Station AP passphrase | The private station AP password, not the upstream Wi-Fi password |
| UI computer IPv4 / HTTP port | `192.168.78.10` / `8000` |
| Device ID | `iot-c6`, matching the PC server's `--device-id` |
| Command polling interval | 500 ms by default |
| Addressable RGB LED GPIO | 8 on the matching C6 DevKit board |

The first build downloads pinned `espressif/cjson` 1.7.19~2. On a machine with
limited RAM, use `IDF_PY_BUILD_JOBS=2 idf.py build`. Flash only the third
board's port; station and bridge keep their existing firmware. Both the
ignored sdkconfig and resulting binaries contain the AP password; do not
publish them. The C6-WROOM-1 module name alone does not guarantee an RGB LED
on a carrier board: confirm its GPIO if using a different development board.

## Behavior and verification

- Starts with the controllable RGB LED off. `led_on` drives green; `led_off`
  turns it off. The separate power LED can stay lit.
- Opens outbound HTTP requests to the PC. No HTTP server runs on Board C,
  and no router port forwarding is needed. PC and target communicate locally
  on the station AP, even without an internet connection.
- Reports its last successfully driven output and confirms commands by ID.
  The website reports success only after a matching acknowledgement. Duplicate
  delivery does not apply the same command twice during a device boot.
- Wi-Fi/HTTP outages retain the output and retry with bounded delays. New
  device boot IDs invalidate pending commands; expired work is not queued
  for later replay. A lost confirmation can leave an unknown result, which
  the website states explicitly and reconciles with later status reports.

For hardware acceptance, join the PC and Board C to IoT-Gateway, start the
service, open the page from the PC or a phone, and try both LED buttons.
Unplug/reconnect Board C and restart the PC service; check availability,
login renewal and LED status after each recovery. Leave station/bridge UART
wiring as it is. This completes a local Wi-Fi application test; the gateway's
upstream routing is tested separately.

The C6 firmware build, sanitized C response-parser tests, ten real-HTTP server
tests, and twelve Postman requests / 23 assertions passed on 2026-11-05.
The HTTP tests use a protocol-level device actor, not C6 hardware. This image
has not yet been flashed onto the third board; its radio, LED and Windows
integration still need the acceptance steps above.

See [device API](../../docs/device-api.md), [browser API](../../docs/api-contract.md)
and [test commands](../../tools/test/README.md). This lab application does not
claim compliance with the original command-isolation architecture.
