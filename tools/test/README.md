# tools/test: evaluation and attack scripts

Scripts that check the gateway against requirements R1–R7. They run against
real hardware or the mock server (`web/mock/server.py`), where the API
contract allows.

> Run attack scripts **only** against the project's own lab setup. Never
> point them at campus or third-party systems.

## Current owner (provisional)

Shared. The project lead reviews.

## Test categories

The four categories below come from the evaluation plan in the first submittal
(section V). Rename them here and in the directory names together if the plan
is renumbered.

| # | Category | Requirements | Scripts |
|---|---|---|---|
| 1 | Authorized operation | R6 | `login_success.py`, `status_reads.py`, `command_applies.py` |
| 2 | Unauthorized access | R2, R7 | `no_token_refused.py`, `bad_password_refused.py`, `forged_token_refused.py`, `token_replay_after_logout.py`, `lockout_after_failures.py`, `restart_invalidates_sessions.py` |
| 3 | Policy and isolation | R1, R3, R4 | `invalid_json_rejected.py`, `unknown_path_rejected.py`, `wrong_method_rejected.py`, `oversized_body_rejected.py`, `invalid_action_rejected.py`, `event_detail_has_no_secret.py` |
| 4 | Rate limits and recovery | R4, R6, R7 | `command_rate_limit.py`, `link_down_refuses_command.py`, `target_unreachable_refuses_command.py`, `recovery_after_link_restore.py` |

Directory layout:

```
tools/test/
├── common.py                  shared client and PASS/FAIL reporter
├── run_all.py                 runs every case script
├── 1-authorized-operation/
├── 2-unauthorized-access/
├── 3-policy-and-isolation/
└── 4-rate-limits-and-recovery/
```

## Running

Against the mock server:

```sh
MOCK_PASSWORD=lab-password python3 web/mock/server.py &
GATEWAY_USER=operator GATEWAY_PASSWORD=lab-password python3 tools/test/run_all.py
```

Against the real gateway, pass the base URL as the first argument:

```sh
GATEWAY_USER=... GATEWAY_PASSWORD=... python3 tools/test/run_all.py https://192.168.1.50
```

A single case can be run on its own, for example:

```sh
python3 tools/test/2-unauthorized-access/lockout_after_failures.py
```

## Conventions

- One script per test case, named after what it checks
  (e.g. `login_lockout.py`).
- Each script prints PASS/FAIL per check and exits non-zero on any failure.
- Credentials come from environment variables (`GATEWAY_USER`,
  `GATEWAY_PASSWORD`), never from arguments that end up in shell history, and
  never from committed files (R2).
- Record results (date, firmware commit, outcome) in the test plan, not in
  this directory.
- A dropped connection is reported as status `0` and counts as a failure,
  because the contract requires a JSON error body for every rejected request.
