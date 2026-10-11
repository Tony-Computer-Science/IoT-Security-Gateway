## What this changes

<!-- One or two sentences. What and why, not how. -->

## Area(s) touched

- [ ] station-board
- [ ] bridge-board
- [ ] target-device
- [ ] web
- [ ] hardware
- [ ] docs / contracts
- [ ] tools / repo config

## Requirements affected

<!-- e.g. "R4: adds failed-login lockout". Write "none" if purely internal. -->

## How it was tested

<!-- Commands run, hardware used, mock-server injections exercised. -->

## Checklist

- [ ] Branch is up to date with `main`
- [ ] Builds: `idf.py build` for each touched board / `python3 web/scripts/build.py` for web
- [ ] No credentials, tokens, keys, or certificates in the diff (R2)
- [ ] If `docs/api-contract.md` or `docs/spi-protocol.md` changed: version bumped, changelog updated, both sides of the interface requested as reviewers
- [ ] If the REST contract changed: `web/mock/server.py` updated to match
- [ ] Area owner(s) requested as reviewer(s)
