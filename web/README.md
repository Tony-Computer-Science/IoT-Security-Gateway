# web/: browser interface

For the real ESP target, use `web/device_server.py`, not the mock. See
[the S3 live-test guide](../docs/s3-live-test.md). This branch retains main's
implemented login/control/events views and integrates the hardware backend.

The single-page web interface that Board A (station board) serves over HTTPS
from a LittleFS partition. It talks to Board A only through the REST API in
[docs/api-contract.md](../docs/api-contract.md).

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
| `mock/` | Python mock of Board A's API with failure injection, see [mock/README.md](mock/README.md) |
| `scripts/build.py` | Minify, gzip, size report, 60 KB budget check |
| `scripts/mklittlefs.sh` | Pack `dist/` into a LittleFS image for the `www` partition |
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

## Build

```sh
python3 web/scripts/build.py        # -> web/dist/*.gz, prints size report
web/scripts/mklittlefs.sh           # -> web/littlefs.bin (needs mklittlefs)
```

`build.py` exits non-zero if the gzipped total is over **60 KB**, or if the
minified output uses `localStorage`, `sessionStorage`, `document.cookie`,
`WebSocket`, or an external `http(s)://` URL.

Board A should serve `/<file>` by reading `/<file>.gz` from LittleFS with
`Content-Encoding: gzip` and the right `Content-Type`. It should also send the
same `Content-Security-Policy` as the mock server (see `CSP` in
`mock/server.py`).

## Constraints (absolute)

These come from running on an ESP32-S3 whose flash and RAM are shared with
mbedTLS and the event log. Don't work around them. If one seems wrong, raise
it with the team.

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

## What is stubbed

The login, control, events and polling views are implemented. Live-device mode
uses heartbeat/service labels; the mock retains its simulated board-link view.
