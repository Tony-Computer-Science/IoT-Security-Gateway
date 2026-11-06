# web/mock: mock gateway API

A development stand-in for the separate web service and its IoT target.
It implements every endpoint in
[docs/api-contract.md](../../docs/api-contract.md) (v0.2.0) with real behavior,
and serves `web/src` from the **same port**, so there is no CORS. Python 3
standard library only.

**Mock only.** The `/__mock/*` endpoints must never exist in firmware.
For the physical third C6, run [the device service](../README.md#control-the-third-c6)
instead. This mock simulates successful LED changes; it does not model the
real device service's 504 acknowledgement failures.

## Current owner (provisional)

Lalit Dhakal. Any member may use or change it. Changes to the REST contract
must update this mock in the same PR.

## Run

```sh
python3 web/mock/server.py                  # http://127.0.0.1:8000/
python3 web/mock/server.py --port 9000      # other port
python3 web/mock/server.py --quiet          # no per-request log lines
```

On startup it prints the URL, the username, and the password.

**Credentials.** The username is `operator` (from `fixtures.json`). The
password is **never stored in the repo** (R2). By default it is randomly
generated on each run and printed at startup. For scripted tests, set
`MOCK_PASSWORD`:

```sh
MOCK_PASSWORD='dev-only-pass' python3 web/mock/server.py
```

## Simulated behavior

Values come from [fixtures.json](fixtures.json). They are **not** the firmware's
limits (see contract open question Q4).

| Behavior | Mock value |
|---|---|
| Session lifetime | `token_ttl_s` = 900 s |
| Lockout | after `max_failed_logins` = 5 consecutive failures, for `lockout_s` = 60 s |
| Command rate limit | `max` = 10 commands per `window_s` = 10 s (global) |
| Event log | ring buffer, `event_log_capacity` = 200 entries |
| Max request body | `max_body_bytes` = 1024 → 413 |
| Unknown `/api/` path / wrong method | 404 / 405 |

## Using the API with curl

```sh
BASE=http://127.0.0.1:8000

# Log in and keep the token in a shell variable
TOKEN=$(curl -s -X POST $BASE/api/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"operator","password":"<printed password>"}' \
  | python3 -c 'import sys,json; print(json.load(sys.stdin)["token"])')

curl -s $BASE/api/status -H "Authorization: Bearer $TOKEN"
curl -s -X POST $BASE/api/command -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' -d '{"action":"led_on"}'
curl -s "$BASE/api/events?limit=10&outcome=denied" -H "Authorization: Bearer $TOKEN"
curl -s -i -X POST $BASE/api/logout -H "Authorization: Bearer $TOKEN"
```

## Failure injection

All injection endpoints accept a JSON object and return the full mock state.
Several keys may be combined in one request. Unknown keys or wrong types
return 400.

### Link down: `{"link_ok": false}` (R7)

The SPI link to Board B is down. Commands return **503 `link_down`**, and
`/api/status` reports `"link_ok": false`.

```sh
curl -s -X POST $BASE/__mock/state -d '{"link_ok": false}'
curl -s -i -X POST $BASE/api/command -H "Authorization: Bearer $TOKEN" \
  -d '{"action":"led_on"}'                       # -> 503 {"error": "link_down"}
curl -s -X POST $BASE/__mock/state -d '{"link_ok": true}'    # restore
```

### Target unreachable: `{"target_reachable": false}` (R6)

Board B cannot reach the target. Commands return **502 `target_unreachable`**
and log a `target` / `alert` event. The LED state is unchanged.

```sh
curl -s -X POST $BASE/__mock/state -d '{"target_reachable": false}'
curl -s -X POST $BASE/__mock/state -d '{"target_reachable": true}'   # restore
```

### Slow gateway: `{"latency_ms": 1500}`

Adds a delay before **every** `/api/*` response (0–30000 ms), so you can see the
UI's pending state.

```sh
curl -s -X POST $BASE/__mock/state -d '{"latency_ms": 1500}'
curl -s -X POST $BASE/__mock/state -d '{"latency_ms": 0}'            # restore
```

### Forced lockout: `{"force_lockout": true}` (R4)

Locks login immediately, as if too many attempts had failed. Login returns
**429 `locked_out`** with `retry_after` for `lockout_s` seconds. `false` clears
the lockout and the failure counter.

```sh
curl -s -X POST $BASE/__mock/state -d '{"force_lockout": true}'
curl -s -X POST $BASE/__mock/state -d '{"force_lockout": false}'     # restore
```

### Reset: `POST /__mock/reset` (R2, R7)

Simulates a gateway restart. Restores every value in `initial_state`,
**invalidates all sessions**, clears the event log and lockout, and restarts
`uptime_s` from 0. Clients holding a token get 401 on their next call.

```sh
curl -s -X POST $BASE/__mock/reset
```

### Inspect: `GET /__mock/state`

```sh
curl -s $BASE/__mock/state
```
