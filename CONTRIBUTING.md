# Contributing

## GitHub usernames: fill these in first

[.github/CODEOWNERS](.github/CODEOWNERS) uses placeholder handles. Once you
know each member's GitHub username, replace every placeholder below in **both**
this table and CODEOWNERS in one pass (`grep -rn "@TODO-" .`).

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
  update `.github/CODEOWNERS` **and** the affected directory README's
  "Current owner (provisional)" section in the same pull request.
- The CODEOWNERS rules are commented out for now so they can't auto-block
  PRs. Uncomment them once the team has confirmed assignments.
- Owner names go in READMEs and CODEOWNERS only, never in source files or
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

Examples: `web/mock-server`, `station/https-server`, `bridge/spi-framing`.

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
   - firmware: `idf.py build` passes for every board you touched;
   - web: `python3 web/scripts/build.py` passes (it fails when the 60 KB
     budget is exceeded or a forbidden API is used);
   - docs: links resolve.
5. Fill in the PR template, including the requirements (R1–R7) the PR affects.
6. Prefer squash-merge for topic branches so `main` history reads as one line
   per change.

### Changes to shared contracts

[docs/api-contract.md](docs/api-contract.md) (web ↔ Board A) and
[docs/spi-protocol.md](docs/spi-protocol.md) (Board A ↔ Board B) are each
implemented by more than one person. A PR that changes either one must:

- bump the version and add a changelog entry;
- be reviewed by the owners on **both** sides of the interface;
- update the mock server (`web/mock/`) in the same PR when the REST contract
  changes.

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
