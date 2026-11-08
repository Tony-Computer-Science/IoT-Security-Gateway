# auth

Legacy scaffold from the former onboard API design. Its proposed role was
credential validation and session management. It is not started and is not a
plan to host the website/API on station. The current separate service reuses
session/login logic from `web/mock/server.py`; see
[Setting Up](../../../../README.md#setting-up).

**Requirements:** R2

## Current owner (provisional)

Antonio (Tony) Goncalves. Any member may work here; the current owner reviews. Reassign by
updating this section and any CODEOWNERS file added later in the same PR.

## Status

Unused stub. `auth_init()` returns `ESP_ERR_NOT_SUPPORTED`. The retained
historical API is in `include/auth.h`.

## Historical design notes

These describe the former proposal, not implemented firmware behavior.

- Tokens and credentials must never be written to the event log or serial output.
- The proposed failed-attempt counter and lockout policy belonged in `policy`;
  this component would report outcomes to it. Both remain unused stubs.
- TODO: how the credential is provisioned (NVS, menuconfig, flash-encrypted?).

## Build

Built as part of the `station-board` ESP-IDF project (`idf.py build` in
`firmware/station-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
