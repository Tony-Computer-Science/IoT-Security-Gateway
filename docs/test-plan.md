# Evaluation test plan and results

Owner: Tiancan Ni (task 30 and 32 in the project schedule).
Status: mock baseline recorded, hardware runs pending.

This document is the record that [tools/test](../tools/test/) writes into. The
scripts check behaviour; the results live here, with the date and the firmware
commit they were run against.

## Scope

The gateway is checked against requirements R1 to R7 in
[requirements.md](requirements.md), using the four categories of the evaluation
plan in the first submittal, section V.

| # | Category | Requirements | What it establishes |
|---|---|---|---|
| 1 | Authorized operation | R6 | A legitimate user can still read status and change the output through the gateway |
| 2 | Unauthorized access | R2, R7 | A client without a valid session cannot read or command the target |
| 3 | Policy and isolation | R1, R3, R4 | Malformed input is rejected, unknown paths and methods are refused, and no secret reaches the event log |
| 4 | Rate limits and recovery | R4, R6, R7 | Bursts are limited, failures are reported as link or target problems, and the gateway recovers without a restart |

## Method

Each case is one script under `tools/test/`, standard library only. A script
prints PASS/FAIL per check and exits non-zero if any check fails, so the whole
suite can run unattended.

```sh
MOCK_PASSWORD=lab-password python3 web/mock/server.py &
GATEWAY_USER=operator GATEWAY_PASSWORD=lab-password python3 tools/test/run_all.py
```

Against hardware, pass the base URL:

```sh
GATEWAY_USER=... GATEWAY_PASSWORD=... python3 tools/test/run_all.py https://192.168.1.50
```

The mock is not authoritative for any limit (see Q4 in
[api-contract.md](api-contract.md)). Its values are taken from
`web/mock/fixtures.json`, and the scripts read the same file, so a change there
does not require a change here.

## Results

| Run | Date | Target | Firmware commit | Outcome |
|---|---|---|---|---|
| Mock baseline | 2026-11-02 | `web/mock/server.py`, API contract v0.1.0 | mock only, no firmware | 18 of 19 case scripts pass |
| Hardware | TODO | Board A over HTTPS | TODO | TODO |

### Category 1, authorized operation

| Case | Requirement | Expected | Result |
|---|---|---|---|
| `login_success` | R2 | 200 with a token and `expires_in` | pass |
| `status_reads` | R6 | 30 of 30 reads succeed, every contract field present | pass, 30 of 30 |
| `command_applies` | R6 | `led_on` then `led_off` reach the target and status agrees | pass |

### Category 2, unauthorized access

| Case | Requirement | Expected | Result |
|---|---|---|---|
| `no_token_refused` | R2 | 401 `invalid_session`, no state in the body | pass |
| `bad_password_refused` | R2 | 401 `invalid_credentials` with `attempts_remaining` | pass |
| `forged_token_refused` | R2 | 401 for every token the gateway never issued | pass |
| `token_replay_after_logout` | R2 | logout returns 204, the old token is then refused | pass |
| `lockout_after_failures` | R4 | five 401s, then 429 `locked_out` with `retry_after` | pass, `retry_after` 60 s |
| `restart_invalidates_sessions` | R7 | a token issued before the restart is refused | pass |

### Category 3, policy and isolation

| Case | Requirement | Expected | Result |
|---|---|---|---|
| `invalid_json_rejected` | R3 | 400 `invalid_request` for four malformed bodies | pass |
| `unknown_path_rejected` | R3 | 404 `not_found` for four unknown paths | pass |
| `wrong_method_rejected` | R3 | 405 `method_not_allowed` for four wrong methods | pass |
| `oversized_body_rejected` | R3 | 413 `payload_too_large` at 1 KB and 4 KB | pass |
| `invalid_action_rejected` | R3 | 400 `invalid_action`, target unchanged | **fail, see below** |
| `event_detail_has_no_secret` | R2 | no password or token appears in any event detail | pass |

### Category 4, rate limits and recovery

| Case | Requirement | Expected | Result |
|---|---|---|---|
| `command_rate_limit` | R4 | the burst is accepted up to the limit, then 429 `rate_limited` | pass, 10 accepted then 429 |
| `link_down_refuses_command` | R7 | 503 `link_down`, target unchanged | pass |
| `target_unreachable_refuses_command` | R6 | 502 `target_unreachable`, target unchanged | pass |
| `recovery_after_link_restore` | R7 | commands work again once the link returns, no restart | pass |

## Deviations

**Mock drops the connection on a non-string `action`.** Sending
`{"action": []}` or `{"action": {}}` to `POST /api/command` closes the
connection instead of returning `400 invalid_action`, which
[api-contract.md](api-contract.md) requires. `web/mock/server.py` compares the
value against a dict, so an unhashable value raises `TypeError` inside the
handler. Tracked as issue #3; `invalid_action_rejected.py` reports it as a
failure until the mock is fixed. This is a defect in the mock, not in the
firmware, and the same case will be re-run against Board A.

## What this contributes to the final report

Section V of the report reports the same four categories. The tables above map
directly:

| Report content | Source |
|---|---|
| Test harness description and how to reproduce it | this document, Method |
| Unauthorized-access results | Category 2 table |
| Malformed-request and policy results | Category 3 table |
| Rate-limit and recovery results | Category 4 table |
| Limitations | deviations above, plus the mock-versus-firmware gap |
