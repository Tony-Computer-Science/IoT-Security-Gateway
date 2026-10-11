# auth

Validates the provisioned credential on `POST /api/login`, issues session tokens, checks the bearer token on every other endpoint, expires sessions, and drops all sessions on restart.

**Requirements:** R2

## Current owner (provisional)

Antonio (Tony) Goncalves. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `auth_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/auth.h`; expand it as the design firms up.

## Notes

- Tokens and credentials must never be written to the event log or serial output.
- The failed-attempt counter and lockout policy live in `policy`; this component reports outcomes to it.
- TODO: how the credential is provisioned (NVS, menuconfig, flash-encrypted?).

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
