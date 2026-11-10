# REST API contract

**Version: 0.4.0**

This is the interface between the web interface (`web/`) and the **separate
web service**, hosted outside the ESP32 boards. For simplicity, the demo runs
the service on a local computer instead of a dedicated server. The mock
(`web/mock/server.py`) implements the browser endpoints for development.
Any contract change follows the process in
[CONTRIBUTING.md](../CONTRIBUTING.md#changes-to-shared-contracts): bump the
version, add a changelog entry, get review from both sides, and update the mock.

The service (`web/device_server.py`) serves the browser endpoints and static
website, backed by an outbound-polling Board C. Its device channel is specified
in [device API](device-api.md). Follow [Setting Up](../README.md#setting-up)
for the fixed-IP PC demo and [S3 live test](s3-live-test.md) for S3 bench details.
Board A and Board B provide networking; Board C controls
the LED. The real service authenticates the target through mutual TLS and
restricts browser access to the hosting PC. This does not establish the
original gateway-only security architecture or physical isolation acceptance.

Versioning follows semver. Before 1.0.0, a minor bump (0.x.0) may break
compatibility, and a patch bump (0.0.x) must not.

---

## 1. Conventions

| Rule | Detail |
|---|---|
| Transport | PC-local browser traffic uses HTTP port 8000; the separate device channel uses mutual TLS on 8443. Mock: HTTP localhost. |
| PC admission | UI/API connections must originate from the bound PC address or IPv4 loopback. Other sources receive 403 `{ "error": "pc_only" }`, including before login. The CLI mock applies the same browser restriction. |
| Encoding | All request and response bodies are JSON (`Content-Type: application/json`), UTF-8. |
| Authentication | Every endpoint except `POST /api/login` requires `Authorization: Bearer <token>`. |
| Paths | Clients always use **relative** paths such as `/api/status`. Never hardcode a scheme, host, or port. |
| Token storage | Clients keep the token in memory only. Never in `localStorage`, `sessionStorage`, cookies, or the URL (R2). |
| Errors | Error responses have a body `{ "error": "<code>" }`, plus the extra fields listed per endpoint. Clients branch on `error`, not on message text. |
| Units | All durations are in **seconds** unless the field name says otherwise. |

---

## 2. Endpoints

### 2.1 `POST /api/login`

Exchanges a provisioned credential for a session token. The only endpoint that
does not require a bearer token.

Request:

```json
{ "username": "string", "password": "string" }
```

| Status | Body | Meaning |
|---|---|---|
| 200 | `{ "token": string, "expires_in": number }` | Success. `expires_in` is the session lifetime in seconds. |
| 401 | `{ "error": "invalid_credentials", "attempts_remaining": number }` | Wrong username or password. `attempts_remaining` is the number of failures left before lockout. |
| 429 | `{ "error": "locked_out", "retry_after": number }` | Too many failed attempts. `retry_after` is seconds until login is allowed again. |

### 2.2 `POST /api/logout`

Invalidates the current session token. No request body.

| Status | Body | Meaning |
|---|---|---|
| 204 | *(none)* | Session invalidated. |
| 401 | `{ "error": "invalid_session" }` | Token missing, unknown, or expired. |

### 2.3 `GET /api/status`

Reads service and target status. Clients poll this at a fixed interval
(default 2000 ms). There is no push channel.

| Status | Body | Meaning |
|---|---|---|
| 200 | see below | Current status. |
| 401 | `{ "error": "invalid_session" }` | Token missing, unknown, or expired. |

200 body:

```json
{
  "output": "on" | "off" | null,
  "target_reachable": boolean,
  "link_ok": boolean,
  "uptime_s": number,
  "seq": number
}
```

| Field | Meaning |
|---|---|
| `output` | Last known LED state on the target. |
| `target_reachable` | The service received a valid Board C heartbeat within the previous eight seconds. |
| `link_ok` | Device-channel availability, using the same heartbeat deadline. Does not measure the A/B SPI/SPI/UART link. |
| `uptime_s` | Seconds since the web service started. A decrease means the service restarted. |
| `seq` | Sequence number of the most recent event-log entry, or 0 before the first event. Lets clients notice new events without fetching `/api/events`. |

`output` is the latest device-reported state; before the first heartbeat,
its value is null and reachability is false. Heartbeats do not prove
internet connectivity. The UI remains usable for local device control when
bridge/internet access is down but the AP and service computer are reachable.
The standalone mock can inject `link_ok` and `target_reachable` independently;
its original A/B-link simulation does not describe the real service's path.

Live-device responses also include `status_source: "device_http"`, `last_seen_s`
(seconds since the latest report, or null) and `device_uptime_s` (uptime at that
report, or null). Device uptime is not extrapolated when reports stop.

Both the real service and mock include `last_confirmed_command`: null until a
command succeeds, otherwise `{ "action": "led_on" | "led_off", "output": "on" |
"off", "service_uptime_s": number }`. The timestamp is the service's uptime
when it accepted the successful command confirmation, not a wall-clock time.
Only a matching successful acknowledgement creates a record in the real service;
the mock simulates confirmation. Failed, pending, denied and unconfirmed commands,
including acknowledgements arriving after timeout, do not replace it. The record
is shared across sessions, retained across device restarts, and cleared by service
restart. It is historical evidence, not a claim about current LED state. Older
services may omit this additive field; clients must tolerate its absence.

The real service also includes `monitoring`, the live gateway checks it runs
itself (the mock omits it):

```json
"monitoring": {
  "enabled": boolean,
  "now_s": number,
  "window_s": 300,
  "diagnosis": "forwarding" | "board_a_down" | "link_or_upstream_down" | "checking" | "not_monitored",
  "board_a":      { "state": S, "target": "192.168.78.1", "latency_ms": number | null, "checked_s_ago": number | null, "history": H },
  "gateway_path": { "state": S, "target": "1.1.1.1:443",  "latency_ms": number | null, "checked_s_ago": number | null, "history": H },
  "device":       { "state": S, "history": H }
}
```

`S` is `"unknown" | "up" | "down" | "off"` (`off`: checks disabled). `H` is the
list of state transitions `[{ "t": number, "state": S }]`, oldest first, where
`t` is seconds since service start on the same base as `now_s`. The first entry
may precede the window and gives the state in effect when it opens.

| Check | Method | What a pass shows |
|---|---|---|
| `board_a` | ICMP echo to Board A's AP address every 2 s | Board A is powered and its AP is up. Board A answers gateway pings even while the link is down. |
| `gateway_path` | TCP connect to an outside IPv4:port every 5 s | Board A forwards (link ready), the SPI/UART link carries traffic, and Board B's upstream and NAT work. It cannot say which of these failed. |
| `device` | Board C heartbeat (8 s deadline) | The device is polling the service. `unknown` until the first report. |

A check turns `down` after two consecutive failures and `up` after one success.
Transitions are logged as `gateway` events. The path check is meaningful only
while the service computer routes that address through Board A. Start the
service with `--no-gateway-checks` for a bench test without Boards A and B;
`--gateway-ip` and `--outside` change the targets.

### 2.4 `POST /api/command`

Requests an output change on the target.

Request:

```json
{ "action": "led_on" | "led_off" }
```

| Status | Body | Meaning |
|---|---|---|
| 200 | `{ "result": "ok", "output": "on" \| "off" }` | Command applied. `output` is the resulting state. |
| 400 | `{ "error": "invalid_action" }` | `action` missing or not in the allow-list. |
| 401 | `{ "error": "invalid_session" }` | Token missing, unknown, or expired. |
| 403 | `{ "error": "not_permitted" }` | Well-formed command refused by policy. |
| 429 | `{ "error": "rate_limited", "retry_after": number }` | Command rate limit exceeded. |
| 502 | `{ "error": "target_unreachable" }` | Target heartbeat is stale, or the pending command was never delivered to the device. |
| 502 | `{ "error": "command_failed", "output": "on" \| "off" }` | PC mode: the device acknowledged this exact command but reported a failure or a different output. `output` is the device's latest report. |
| 503 | `{ "error": "server_busy", "retry_after": 1 }` | PC mode: browser/API worker slots are full; the request was not handled. Applies to every non-device endpoint. |
| 503 | `{ "error": "link_down" }` | Standalone mock's injected link failure; not emitted by the current real-device service. |
| 504 | `{ "error": "command_unconfirmed" }` | PC mode delivered a command but received no matching acknowledgement (timeout or device restart). The state may have changed. |

Admission failures and commands that were never delivered do not apply a
state change. A **504**, client/network timeout, or lost response after
delivery means the outcome is unknown; do not claim the LED was unchanged
or retry automatically. Poll status for the latest device report. A 200 in
PC mode is sent only after the device acknowledges the matching command ID
and reports the requested state. This is a driver/state acknowledgement,
not optical LED feedback. The original synchronous mock still changes its
simulated state only on successful commands.

### 2.5 `GET /api/events?limit=<n>&outcome=<allowed|denied|alert>`

Reads the bounded event log, newest first. Both query parameters are optional.

| Param | Meaning |
|---|---|
| `limit` | Maximum entries to return. Positive integer. Upper bound set by the server (log capacity). |
| `outcome` | Return only entries with this outcome. |

| Status | Body | Meaning |
|---|---|---|
| 200 | see below | Matching events. |
| 401 | `{ "error": "invalid_session" }` | Token missing, unknown, or expired. |

200 body:

```json
{
  "events": [
    {
      "seq": number,
      "uptime_s": number,
      "type": "auth" | "command" | "gateway" | "rate_limit" | "target",
      "outcome": "allowed" | "denied" | "alert",
      "detail": string
    }
  ]
}
```

`detail` is human-readable text. It must **never** contain a password, a
session token, or any other secret (R2).

In PC mode every refused or failed browser request writes exactly one event, so
blocked attempts are visible (R5). Details name only the validated action;
client-supplied values are never echoed.

| Request outcome | type / outcome | detail |
|---|---|---|
| Missing or invalid session | auth / denied | `request with missing or invalid session` |
| Wrong credentials, lockout | auth / denied, rate_limit / alert | `login failed (n/max)`, `login attempt during lockout`, `login locked out for Ns` |
| Malformed or oversized login body | auth / denied | `malformed login request rejected` / `oversized login request rejected` |
| Malformed or oversized command body | command / denied | `malformed command request rejected` / `oversized command request rejected` |
| Unknown action or extra fields | command / denied | `invalid action rejected` |
| Command rate limit | rate_limit / alert | `command rate limit exceeded` |
| Another command in flight | command / denied | `<action> rejected: previous command still awaiting device` |
| Device not reporting | command / denied | `<action> rejected: device unreachable` |
| Admitted, never delivered | command / alert | `<action> not delivered: device stopped polling` |
| Delivered, no acknowledgement | command / alert | `<action> unconfirmed: no device acknowledgement` |
| Device reported failure | command / alert | `<action> failed: device reported output <on/off>` |
| Confirmed | command / allowed | `<action> confirmed by device` |

`503 server_busy` refusals are not logged per request: under overload that would
evict earlier events from the bounded log. The log holds the newest
`event_log_capacity` entries (200 by default), so a sustained flood of refused
requests can still push older events out.

---

## 3. Common errors (all endpoints)

R3 requires the gateway to reject unknown paths, unknown methods, invalid values,
and oversized inputs. These responses apply to every path under `/api/`:

| Status | Body | When |
|---|---|---|
| 400 | `{ "error": "invalid_request" }` | Body is not valid JSON or is missing required fields; invalid query parameter. |
| 404 | `{ "error": "not_found" }` | Unknown path. |
| 405 | `{ "error": "method_not_allowed" }` | Known path, wrong method. |
| 413 | `{ "error": "payload_too_large" }` | Body larger than the server limit. |

The mock and real-device service implement these with a 1024-byte request-body
limit. No browser REST server is part of the board firmware.

---

## 4. Current implementation and open questions

- **Q1 (implemented):** `seq` is the newest event-log sequence number in both
  the mock and real service. It starts at 0 and resets on service restart.
- **Q2:** `403 not_permitted` has no trigger yet, since there is only one role.
  Keep it as reserved, or define when it applies.
- **Q3 (implemented):** Command limits are global, shared across browser
  sessions. The real service additionally permits only one pending command;
  contention returns 429 with `retry_after=1`. The mock counts command attempts
  before action/reachability checks; the real service counts admitted commands
  after validating the action, available slot and fresh heartbeat.
- **Q4:** Deployment-specific values for token lifetime, lockout threshold
  and duration, command rate limit, event-log capacity, and maximum body size.
  The local service currently reuses `web/mock/fixtures.json`; dedicated-server
  deployment settings remain a separate decision.

Current lab defaults used by both services:

| Setting | Value |
|---|---|
| Browser username | `operator` |
| Session lifetime | 900 seconds |
| Login lockout | Five consecutive failed logins trigger 60 seconds of lockout |
| Command sliding window | 10 commands per 10 seconds, global |
| Event log | 200 entries; default query limit 50 |
| Request body limit | 1024 bytes |

The failure that reaches the login threshold returns 401 with
`attempts_remaining=0`; subsequent attempts during lockout return 429.
The real service's five-second command wait and eight-second heartbeat
deadline are separate settings in `DeviceGateway`, not fixture values. These
are current lab service limits, not firmware-wide security guarantees.

---

## 5. Changelog

### 0.4.0 — PC-only access and authenticated target

- Restrict UI/API access to the hosting PC; remote AP browsers receive 403
  `pc_only`. Login, control and events retain their existing fields and behavior
  for permitted PC clients. The CLI mock enforces the same admission rule.
- Move real device polls to mutual TLS, as specified by device API 0.2.0.
  A plaintext poll cannot create or refresh target state.

### 0.3.3 — live gateway monitoring

- The real service checks Board A by ping and the path through the SPI/UART link
  and Board B by a TCP connection to an outside address, and reports them with
  five-minute histories in `status.monitoring`, alongside the device heartbeat.
- Add the `gateway` event type for check transitions.
- A background tick now expires a silent device even when no browser polls.

### 0.3.2 — distinct device failures and reserved device capacity

- Add 502 `command_failed` when the device acknowledges a command but reports
  failure or a different output; 504 `command_unconfirmed` now means only that
  no matching acknowledgement arrived.
- Add 503 `server_busy`: in PC mode, browser/API requests may use at most
  `total - device` worker slots (default 16 of 24), so they cannot block device
  polls. A slow-header flood before the request line is not addressed.
- Align the browser client's declared version with the service and mock.

### 0.3.1 — live monitoring details

- Add shared last-confirmed-command history to status in the service and mock.
- Document device uptime and report age; distinguish stale telemetry and command
  history from the current device report. Device wire protocol is unchanged.

### 0.3.0 — live target integration

The real-device service adds `status_source: "device_http"`, `last_seen_s`
(seconds since the last accepted device report, or null before first contact),
and `device_uptime_s` (uptime in the last report, or null). Before any report,
`output` is null rather than a fabricated off state. After expiry it retains
the last report; clients must inspect reachability/freshness. Device-reported
uptime is a snapshot, not an advancing clock. The standalone mock omits these
optional fields and retains its simulated link/target behavior. Clients select
live-device labels only when `status_source` identifies the real service.


### 0.2.0

- Add PC lab mode using the existing browser endpoints and a separately
  specified outbound device channel.
- Define heartbeat-based reachability/service uptime for PC mode.
- Add 504 `command_unconfirmed`; distinguish ambiguous delivered commands
  from pre-delivery refusal. Update the frontend's error handling and mock version.
- Documentation clarified on 2026-11-06: the web service is external to all
  boards, with a local-computer demo. Existing heartbeat, uptime and error
  behavior is unchanged; onboard hosting is not a planned setup step.

### 0.1.0 (initial)

- Initial contract: `POST /api/login`, `POST /api/logout`, `GET /api/status`,
  `POST /api/command`, `GET /api/events`.
- Common error responses (400/404/405/413) added to satisfy R3.
