# Board C to PC device API

**Version 0.2.0.** One ESP32-C6 or ESP32-S3 target joins IoT-Gateway and initiates
mutually authenticated HTTPS requests to **192.168.78.10:8443**. The website and authenticated browser
API are a separate service external to the boards. For simplicity, the demo
runs this service on a local computer instead of a dedicated server. The PC
serves both from the same origin. It never opens a
connection to Board C. This adds no port forwarding through station/bridge.
See [Setting Up](../README.md#setting-up) for startup order and LED checks.

The demo PC acts as the server at a manually assigned fixed address. Board C
has this address and port compiled into its firmware from `TARGET_UI_HOST`
and `TARGET_UI_PORT` in menuconfig (defaults **192.168.78.10** / **8443**).
There is no automatic server discovery. Moving the service to a dedicated
server with a different endpoint requires rebuilding and flashing Board C
with matching settings. The website and APIs remain on the server, never
on an ESP32.

The PC and target require provisioned certificates. The PC checks both the
private CA and the exact target certificate fingerprint; the target verifies
the PC certificate against that CA and its configured server IP. Plaintext
polls on the browser port receive 403 `device_tls_required`. The TLS listener
does not expose browser login/control/events routes. Device IDs remain labels,
not credentials. No unauthenticated fallback exists.

Provision once on the PC with `python3 tools/provision_device_tls.py`, then
build the target using that private identity directory. Its CA certificate,
client certificate and client key are embedded; keep firmware and generated
files private. The CA signing key and PC key stay on the PC. Certificate
rotation requires provisioning a new directory and rebuilding the target.

Target certificate date checks are enabled. Before its first connection, the
target bootstraps its clock from firmware build time; after authenticated TLS
it refines that clock using the PC's `X-Device-Time` response header. The first
connection relies on the build-time clock estimate, not an independent RTC.
Certificate chain and server-name checks are always required. Rebuild after
identity rotation or long storage.

The target's lwIP input hook drops IPv4 packets from addresses other than the
configured PC. Unfragmented DHCP replies (UDP 67 → 68) and local ICMP echo
(type 8/0, code 0) from unicast sources on the AP /24 are allowed exceptions;
lwIP still validates those packets. IPv6 is disabled. ARP and Wi-Fi management traffic
remain necessary, so this is not AP-wide layer-2 client isolation. A spoofed
PC IP cannot complete the authenticated device channel without the PC key.

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
  off. On is green on the configured addressable RGB LED (GPIO8 on the matching
  C6 carrier; S3 requires a verified carrier GPIO).
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
or timed out. The device remembers its **most recently attempted ID** and
does not reapply consecutive deliveries of that ID, including after a lost
ACK response. It reports that ID/result in subsequent polls. There is no
history of earlier IDs or persistent deduplication across boots. The PC
confirms only the matching delivered ID on the matching boot,
with ack_ok=true and the requested output. Mere command delivery is not success.

The browser command waits up to five seconds. If never delivered, it returns
502 `target_unreachable`. If the device acknowledges that exact ID but reports
ack_ok=false or a different output, it returns 502 `command_failed` with the
reported output. If delivered but never acknowledged, or invalidated by a device
restart, it returns 504 `command_unconfirmed`. In both cases the physical result
is not optically verified. Expired commands are removed; new polls do not fetch them.
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

- 0.2.0: Mutual TLS on the dedicated device listener, exact target certificate
  pinning at the PC, authenticated PC time, PC-only IPv4 filtering and rejection
  of plaintext device polls. DHCP and local unfragmented ping are explicit
  packet-filter exceptions. JSON command/report fields are unchanged.
- 0.1.0: Initial outbound poll, bounded command confirmation, restart and
  acknowledgement behavior for the real Board C lab target.
- Documentation clarified on 2026-11-08: deduplication retains only the most
  recently attempted command ID. No API or wire behavior changed.
