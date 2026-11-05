# REST API contract

**Version: 0.2.0**

This is the interface between the web interface (`web/`) and Board A's firmware
(`firmware/station-board`). The mock server (`web/mock/server.py`) implements
it for development. Any change follows the process in
[CONTRIBUTING.md](../CONTRIBUTING.md#changes-to-shared-contracts): bump the
version, add a changelog entry, get review from both sides, and update the mock.

The PC-hosted lab implementation (`web/device_server.py`) serves the same
browser endpoints and static website, backed by a real outbound-polling Board C.
Its device channel is specified in [device API](device-api.md). No incoming
connection to Board C is needed. The original Board A HTTPS implementation
remains unimplemented; this PC mode does not claim the original gateway-only
security architecture or authenticate the intentionally vulnerable device.

Versioning follows semver. Before 1.0.0, a minor bump (0.x.0) may break
compatibility, and a patch bump (0.0.x) must not.

---

## 1. Conventions

| Rule | Detail |
|---|---|
| Transport | Original Board A design: HTTPS port 443. Mock: HTTP localhost. PC lab mode: HTTP port 8000 on the private station subnet. |
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

Reads gateway and target status. Clients poll this at a fixed interval
(default 2000 ms). There is no push channel.

| Status | Body | Meaning |
|---|---|---|
| 200 | see below | Current status. |
| 401 | `{ "error": "invalid_session" }` | Token missing, unknown, or expired. |

200 body:

```json
{
  "output": "on" | "off",
  "target_reachable": boolean,
  "link_ok": boolean,
  "uptime_s": number,
  "seq": number
}
```

| Field | Meaning |
|---|---|
| `output` | Last known LED state on the target. |
| `target_reachable` | Board B can currently reach Board C over the private network. |
| `link_ok` | The SPI link between Board A and Board B is up and both boards are ready (R7). |
| `uptime_s` | Seconds since Board A booted. A decrease means Board A restarted. |
| `seq` | Sequence number of the most recent event-log entry. Lets clients notice new events without fetching `/api/events`. *(See open question Q1.)* |

In PC lab mode, `link_ok` and `target_reachable` indicate a device heartbeat
within the previous eight seconds. They do not measure the A/B SPI/UART link
or internet connectivity. `uptime_s` is PC service uptime and `output` is the
latest device-reported state. The UI remains usable for local device control
when bridge/internet access is down but the AP and PC are reachable.

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
| 502 | `{ "error": "target_unreachable" }` | Link is up, but Board B could not reach the target. |
| 503 | `{ "error": "link_down" }` | The SPI link or Board B is not ready. Commands are disabled (R7). |
| 504 | `{ "error": "command_unconfirmed" }` | PC mode delivered a command but could not confirm its result, or the device reported an actuator failure. The state may have changed. |

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
      "type": "auth" | "command" | "rate_limit" | "target",
      "outcome": "allowed" | "denied" | "alert",
      "detail": string
    }
  ]
}
```

`detail` is human-readable text. It must **never** contain a password, a
session token, or any other secret (R2).

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

The mock and PC service implement these with a 1024-byte request-body limit.
The original Board A REST firmware body-size limit remains TODO.

---

## 4. Open questions

- **Q1:** Confirm what `seq` in `/api/status` means. The current proposal is
  "seq of the newest event-log entry".
- **Q2:** `403 not_permitted` has no trigger yet, since there is only one role.
  Keep it as reserved, or define when it applies.
- **Q3:** Are command rate limits per session or global? The mock uses global.
- **Q4:** Firmware values for the token lifetime, lockout threshold and
  duration, command rate limit, event-log capacity, and maximum body size.
  The mock's values are in `web/mock/fixtures.json` and are **not**
  authoritative.

---

## 5. Changelog

### 0.2.0

- Add PC lab mode using the existing browser endpoints and a separately
  specified outbound device channel.
- Define heartbeat-based reachability/service uptime for PC mode.
- Add 504 `command_unconfirmed`; distinguish ambiguous delivered commands
  from pre-delivery refusal. Update the frontend's error handling and mock version.

### 0.1.0 (initial)

- Initial contract: `POST /api/login`, `POST /api/logout`, `GET /api/status`,
  `POST /api/command`, `GET /api/events`.
- Common error responses (400/404/405/413) added to satisfy R3.
