# web/: browser interface

The single-page web interface and REST API are hosted by a **separate service
outside the ESP32 boards**. For simplicity, the demo runs that service on a
local computer at 192.168.78.10:8000 instead of a dedicated server. Board C
initiates requests to it over Wi-Fi. See the root
[Setting Up guide](../README.md#setting-up) and
[browser API contract](../docs/api-contract.md).

## Current owner (provisional)

Lalit Dhakal. Any member may work here; the current owner reviews (see
[CONTRIBUTING.md](../CONTRIBUTING.md#ownership-is-provisional)).

## Layout

| Path | Purpose |
|---|---|
| `src/index.html` | Semantic shells for the three views: login, control, events |
| `src/style.css` | Design tokens and the six state styles |
| `src/app.js` | Bootstrap, view registry, show/hide switching, status polling loop |
| `src/api.js` | One function per endpoint; bearer token, JSON, error mapping. No DOM access. |
| `device_server.py` | PC UI/API service with outbound device polling and command acknowledgements |
| `mock/` | Simulated service/device behavior for UI development, see [mock/README.md](mock/README.md) |
| `scripts/build.py` | Minify, gzip, size report, 60 KB budget check |
| `scripts/mklittlefs.sh` | Legacy asset packer from the former onboard-hosting design; unused by the service |
| `dist/` | Build output. **Git-ignored.** Created by `build.py`. |

The build scripts live in `scripts/`, not `build/`. ESP-IDF's ignore pattern
`build/` matches at any depth and would swallow a `web/build/` directory.
Don't rename it.

## Develop

```sh
python3 web/mock/server.py          # then open the printed URL
```

The mock serves `web/src` and the API from one origin, so there is no CORS
and no build step during development. Edit a file and reload.

## Control the third C6

Configure the Windows UI PC as **192.168.78.10/24** on IoT-Gateway using
[these steps](../docs/client-network.md), including the scoped TCP 8000
firewall rule. With Python 3.9 or later, run from the repository root:

```powershell
python web/device_server.py --host 192.168.78.10 --port 8000
```

Open `http://192.168.78.10:8000/`. Sign in as `operator` with the login
password printed by the service, or set `UI_PASSWORD` in the environment
before launch to choose one. No package installation or web build is needed.
This password is separate from the Wi-Fi passphrase.

Service options are `--host` (default `192.168.78.10`), `--port` (default
`8000`), `--device-id` (default `iot-c6`) and `--quiet` (suppress request logs).
Match the address, port and device ID in Board C's menuconfig. When
`UI_PASSWORD` is set, startup prints `(from UI_PASSWORD)` instead of the
password; sign in with the value you configured.

Build and flash [Board C](../firmware/target-device/README.md) with the station
AP credentials and the same PC address/port. Its Wi-Fi requests carry state
reports and receive `led_on` / `led_off` commands. The PC returns browser
success only after a matching device acknowledgement. Unconfirmed delivered
commands return 504; the UI keeps the last confirmed output and reconciles
later reports instead of retrying automatically. Heartbeats expire after
eight seconds. A command waits up to five seconds, with one command in flight.

One IoT target is supported. The PC service disables all `/__mock/` routes.
Its device channel is intentionally unauthenticated plain HTTP on the lab
AP; it does not implement TLS or device identity verification. See the
[device wire contract](../docs/device-api.md) and [automated checks](../tools/test/README.md).

## Build

```sh
python3 web/scripts/build.py        # -> web/dist/*.gz, prints size report
```

`build.py` exits non-zero if the gzipped total is over **60 KB**, or if the
minified output uses `localStorage`, `sessionStorage`, `document.cookie`,
`WebSocket`, or an external `http(s)://` URL.

The current PC service serves `web/src/` directly; these generated assets
are not needed for the setup procedure. The LittleFS packer is retained as
legacy tooling and does not imply a plan to host the website on a board.
The real service and mock send the same `Content-Security-Policy` (see `CSP`
in `mock/server.py`).

## Constraints (absolute)

These lightweight frontend constraints are retained from the earlier design.
They apply to the web assets even though hosting now belongs to the separate
service, with no ESP32 flash or LittleFS deployment step.

- **No framework or library** (React, Vue, Svelte, jQuery, Alpine, htmx, …).
- **No bundler** (Vite, webpack, Rollup, esbuild, Parcel).
- **No TypeScript.** Plain ES2020 modules loaded directly by the browser.
- **No npm runtime dependencies.** Build tooling is Python.
- **No CSS framework, web fonts, CDN, or external requests.** Must work fully
  offline on an isolated network.
- **Session token in memory only.** Never `localStorage` or `sessionStorage`
  (R2). Reloading the page signs the user out. This is intentional.
- **Single page.** Views are toggled with the `hidden` attribute. No router.
- **60 KB gzipped total** for all assets.
- **Polling, not WebSocket.** `GET /api/status` every 2000 ms by default.
- **Relative URLs only** (`/api/status`), never a scheme, host, or port.

## State styles

Every status message uses `<el class="state" data-state="...">label</el>`.
Each state has its own color, glyph, **and** border style, plus a text label,
so no state relies on color alone:

| `data-state` | Glyph | Border | Used for |
|---|---|---|---|
| `ok` | ✓ | solid | success |
| `pending` | … (pulsing) | dashed | request in flight |
| `denied` | ✕ | thick left bar | 400 / 401 / 403 |
| `rate-limited` | ‖ | double | 429 `rate_limited`, `locked_out` |
| `target-unreachable` | ⚠ | dotted | 502 |
| `link-down` | ⨯ | double left bar, striped | 503 |

In the real service, heartbeat expiry sets both availability fields false
and uses the channel-unavailable style; it does not report A/B link health.
A delivered command without confirmation returns 504 and an unknown-result
message; the UI preserves the last reported output until a fresh report arrives.

## Remaining work

Login, status polling and LED control are implemented. The browser events
view remains a TODO; `/api/events` is available from the backend. The PC
service has local HTTP tests, but the physical third C6 and Windows PC still
need an end-to-end test. The website/API belongs to the separate service;
the boards provide networking and the IoT output.
