# Board C to PC device API

**Version 0.1.0.** One ESP32-C6-WROOM-1 target joins IoT-Gateway and initiates
HTTP requests to **192.168.78.10:8000**. The PC serves the existing website
and authenticated browser API from that same origin. It never opens a
connection to Board C. This adds no port forwarding through station/bridge.

Board C remains an intentionally vulnerable **lab-only plain-HTTP device**:
this channel has no device authentication or TLS. Its public device ID is
not a credential. AP peers can impersonate it; the browser's operator login
does not fix that evaluation-target limitation.

## `POST /device/poll`

Required content type: `application/json`. No query parameters. Maximum
request body: 1024 bytes. Firmware responses are bounded to 512 bytes.
Unknown paths return 404; wrong methods return 405; invalid messages return
400; an unexpected device ID returns 403 `unknown_device`.

Request, with exactly these fields:

```json
{
  "device_id": "iot-c6",
  "boot_id": "0123456789abcdef",
  "output": "off",
  "ack_id": null,
  "ack_ok": true,
  "uptime_s": 42
}
```

- `device_id`: 1–32 ASCII letters/digits/underscore/hyphen, matching server
  `--device-id` (default `iot-c6`). One target is supported by this UI contract.
- `boot_id`: fresh random 16-character lowercase hex value each device boot.
- `output`: `on` or `off`, the last successfully driven LED state. Boot starts
  off. On is green on the GPIO8 addressable RGB LED.
- `ack_id`: last attempted command ID, 32 lowercase hex characters, or null
  before any command. Repeated acknowledgements are safe.
- `ack_ok`: true for a successful driver operation; false on driver failure.
  Ignored when ack_id is null. Failed operations do not pretend to succeed.
- `uptime_s`: integer 0–4294967295, seconds since device boot.

Idle response, HTTP 200:

```json
{"command": null}
```

Command response, HTTP 200:

```json
{"command": {"id": "0123456789abcdef0123456789abcdef", "action": "led_on"}}
```

Only `led_on` and `led_off` are accepted. Firmware rejects unknown fields,
duplicate fields, invalid types/IDs/actions, overlong bodies, trailing junk,
JSON escapes and nesting deeper than two containers. The restricted channel
needs no escapes; all names/actions/IDs are ASCII. A partial or invalid HTTP
response never changes the actuator. Redirects are not followed.

## Lifecycle and command confirmation

The device polls every 500 ms while healthy. HTTP failures back off to five
seconds without changing the last successfully set LED state. Wi-Fi retries
use bounded exponential backoff and a 15-second association/DHCP wait. No
open-network fallback exists. Credentials are compiled from ignored sdkconfig
and Wi-Fi runtime storage is RAM. Passwords and message bodies are not logged.

The PC holds **one pending command**; overlapping browser commands receive
429 with `retry_after=1`. Command IDs are randomly generated for every request,
including after PC restarts. A command is associated with the device boot ID
observed when it was admitted. Changing boot ID invalidates old pending work.
There is no persistent replay queue after either side restarts.

While a command is pending, polls return the same command ID until confirmed
or timed out. The device applies each ID at most once within its current boot
and includes that ID/result in subsequent polls, including after a lost ACK
response. The PC confirms only the matching delivered ID on the matching boot,
with ack_ok=true and the requested output. Mere command delivery is not success.

The browser command waits up to five seconds. If never delivered, it returns
502 `target_unreachable`. If delivered but unconfirmed, failed, or invalidated
by a device restart, it returns 504 `command_unconfirmed`. Its physical result
may be unknown. Expired commands are removed; new polls do not fetch them.
Do not auto-retry ambiguous commands. Status polls reconcile later reports.

The PC marks the target/channel unavailable eight seconds after the last
valid poll. It has no hardware knowledge of the separate A/B link. The web
frontend treats this as device reachability, not proof of internet access.

## Running the PC service

On the Windows UI computer, configure the fixed address from
[client-network setup](client-network.md), then from the repository root run:

```powershell
python web/device_server.py --host 192.168.78.10 --port 8000
```

Open `http://192.168.78.10:8000/`. Use username `operator` and the UI login
password printed by the server. It generates one per run; set `UI_PASSWORD`
in the local environment to choose a persistent one. This is separate from
the Wi-Fi passphrase. The existing scoped Windows firewall rule for TCP 8000
applies to this service. Failure injection routes under `/__mock/` are disabled.

For a workstation-only test, bind the service to `127.0.0.1` and use the
simulated-device test harness. Those tests do not validate a physical C6 radio,
LED or the Windows PC. Deploying the third board needs the matching ESP-IDF
6.1 build and private station AP credentials from menuconfig.

## Changelog

- 0.1.0: Initial outbound poll, bounded command confirmation, restart and
  acknowledgement behavior for the real Board C lab target.
