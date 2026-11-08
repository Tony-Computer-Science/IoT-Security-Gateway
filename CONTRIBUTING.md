# Contributing

## Provisional ownership and GitHub usernames

Directory READMEs record provisional owners. This checkout has no
`.github/CODEOWNERS` file or pull request template; request owner reviews
manually. Once the team confirms assignments and GitHub usernames, update
the roster below and create CODEOWNERS if automatic review routing is wanted.

| Member | Placeholder | Real username |
|---|---|---|
| Antonio (Tony) Goncalves | `@TODO-username-goncalves` | TODO |
| Lal Za Lian | `@TODO-username-lian` | TODO |
| Zachariah Grande | `@TODO-username-grande` | TODO |
| Lalit Dhakal | `@TODO-username-dhakal` | TODO |
| Carter Cattadoris | `@TODO-username-cattadoris` | TODO |

Known username: **`DEADSEC-SECURITY`**. TODO: record which member it belongs to
and swap it in for that member's placeholder.

## Ownership is provisional

- **Any member may work in any directory.** Ownership routes reviews. It does
  not restrict access.
- The current owner of an area should review changes to that area (see
  [Pull requests](#pull-requests)).
- Assignments will change as the project goes on. To record a reassignment,
  update the affected directory README's "Current owner (provisional)"
  section in the same pull request. Keep CODEOWNERS in sync if it is added.
- Owner names go in READMEs and any future CODEOWNERS file, never in source files or
  CMakeLists. Code comments describe what a component does, not who writes it.

## Branching

`main` is always buildable. Nobody pushes to `main` directly; every change
arrives through a pull request.

### Topic branches (preferred)

Name: `<area>/<short-description>`, lowercase, hyphens.

| Prefix | Area | Typical author(s) |
|---|---|---|
| `station/` | `firmware/station-board` | Goncalves, Lian |
| `bridge/` | `firmware/bridge-board` | Grande |
| `target/` | `firmware/target-device` | Grande |
| `web/` | `web/` | Dhakal |
| `hw/` | `hardware/` | Cattadoris |
| `docs/` | `docs/` or any README | anyone |
| `tools/` | `tools/` | anyone |
| `chore/` | repo config, `.github/`, cross-cutting setup | anyone |

Examples: `web/device-service`, `station/client-dhcp`, `bridge/spi-framing`.

Keep topic branches short-lived: open the PR within a few days, even as a
draft.

### Personal branches `dev/<name>`

Each member also has a long-lived working branch:

```
dev/goncalves   dev/lian   dev/grande   dev/dhakal   dev/cattadoris
```

**Long-lived personal branches drift away from `main`.** The longer one lives,
the bigger and riskier its eventual merge, and the more likely it is to undo
someone else's work by accident. Rules:

- **Rebase your `dev/` branch onto `main` at least once a week:**
  ```sh
  git fetch origin
  git switch dev/<name>
  git rebase origin/main
  git push --force-with-lease
  ```
- **For anything that takes more than a few days, use a short-lived topic
  branch instead.** Use `dev/<name>` for experiments and scratch work, not as
  a place to pile up a week of unrelated changes.
- Merge to `main` from a `dev/` branch the same way as any other: through a PR.

## Commits

Format: `<area>: <imperative summary>`. Keep the summary under about 72
characters and leave out the trailing period.

```
web: add mock server
station: scaffold auth component
bridge: validate frame length before parsing
docs: bump API contract to 0.2.0
```

Areas: `station`, `bridge`, `target`, `firmware` (affects more than one board),
`web`, `hw`, `docs`, `tools`, `repo` (root config and `.github/`).

When a commit implements or affects a requirement, name it in the body, e.g.
`Implements R4 (failed-login lockout).`

## Pull requests

1. **At least one approving review** before merging.
2. **The area owner must review** changes to their directory. If one PR touches
   several areas, every affected owner reviews.
3. **No direct pushes to `main`.**
4. **Never merge a PR that breaks the build.** Before requesting review:
   - firmware: `idf.py build` passes for every project you touched; shared
     gateway changes also require A, B and the S3 test-host builds plus
     `python3 tools/test/run_host_tests.py`;
   - Board C protocol: build `target-device`, then run
     `python3 tools/test/run_target_tests.py` against its managed cJSON;
   - PC service: `python3 tools/test/test_device_server.py` passes; run
     `python3 tools/test/run_device_api_tests.py` too when Postman is available;
   - web: `python3 web/scripts/build.py` passes (it fails when the 60 KB
     budget is exceeded or a forbidden API is used);
   - docs: links resolve.
5. Describe the behavior changed and validation performed in the PR body,
   including the requirements (R1–R7) affected and any remaining limitations.
6. Prefer squash-merge for topic branches so `main` history reads as one line
   per change.

### Changes to shared contracts

[docs/api-contract.md](docs/api-contract.md) (browser ↔ separate web service) and
[docs/device-api.md](docs/device-api.md) (PC service ↔ Board C), plus the
[SPI](docs/spi-protocol.md) and [UART](docs/uart-link.md) contracts (A ↔ B),
are shared interfaces. A PR that changes wire or API behavior must:

- bump the version and add a changelog entry;
- be reviewed by the owners on **both** sides of the interface;
- update the mock server (`web/mock/`) in the same PR when the REST contract
  changes, and update the real service, affected firmware/frontend and tests.

Documentation corrections that describe existing behavior do not change the
interface version. Keep build/host-test evidence distinct from physical
acceptance; follow [the test guide](docs/testing.md). The original R1–R7 text
is preserved in [requirements.md](docs/requirements.md); the router milestone's
differences must stay explicit in [architecture.md](docs/architecture.md).

### Recommended repository settings (TODO for the project lead)

On GitHub: Settings → Branches → add a protection rule for `main`:
require a pull request, require 1 approval, block force pushes, and (once
CODEOWNERS is active) require review from code owners.

## Secrets (R2)

Never commit credentials, Wi-Fi passwords, session tokens, TLS private keys, or
certificates. `.gitignore` blocks common file patterns, but it doesn't
understand what's inside files, so you are still responsible:

- Wi-Fi SSIDs and passwords are set through `idf.py menuconfig`, which writes
  the git-ignored `sdkconfig`. Never put them in `sdkconfig.defaults`.
- Generate TLS keys and certificates locally. Don't commit them.
- If a secret is committed, tell the team right away and rotate it.
  Removing it in a later commit does **not** remove it from history.

## Code style

[.editorconfig](.editorconfig) sets the basics: UTF-8, LF line endings,
4 spaces for C, CMake, and Python, 2 spaces for JS, HTML, and CSS.
