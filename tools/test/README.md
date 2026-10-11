# tools/test: evaluation and attack scripts

Scripts that check the gateway against requirements R1–R7. They run against
real hardware or the mock server (`web/mock/server.py`), where the API
contract allows.

> Run attack scripts **only** against the project's own lab setup. Never
> point them at campus or third-party systems.

## Current owner (provisional)

Shared. The project lead reviews.

## Test categories

**TODO: copy the four categories verbatim from the team's test plan.** They are
not in the repo yet, so the headings below are placeholders. Rename them
to match the test plan and keep the numbering.

| # | Category (TODO: from test plan) | Requirements | Scripts |
|---|---|---|---|
| 1 | TODO | TODO | none yet |
| 2 | TODO | TODO | none yet |
| 3 | TODO | TODO | none yet |
| 4 | TODO | TODO | none yet |

Proposed directory layout once the categories are known:

```
tools/test/
├── 1-<category>/
├── 2-<category>/
├── 3-<category>/
└── 4-<category>/
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
