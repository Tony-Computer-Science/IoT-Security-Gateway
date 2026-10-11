# firmware/target-device: Board C

> ## ⚠️ WARNING: INTENTIONALLY VULNERABLE DEVICE ⚠️
>
> **This firmware is deliberately insecure.** It uses hardcoded credentials,
> has no authentication, and speaks plain HTTP. It exists **solely as an
> evaluation target** for the IoT Security Gateway project.
>
> - **Never connect this device to a production network, the campus network,
>   eduroam, or any network you do not fully control.**
> - Run it only on Board B's private access point, in the lab.
> - Do not reuse any code from this directory in the gateway boards or any
>   other project.
> - Do not "fix" its vulnerabilities. The gateway is supposed to protect it
>   as it is.

## Purpose

Board C stands in for an existing insecure IoT device that the gateway protects
**without modifying it**. It joins Board B's private Wi-Fi network and
exposes an unauthenticated plain-HTTP interface that controls an LED.

## Current owner (provisional)

Zachariah Grande. Any member may work here; the current owner reviews.

## Build

Same layout as the other boards. It is an ESP-IDF v5.x project with only
`main/`:

```sh
cd firmware/target-device
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

## Status / TODO

- `main.c` only logs a banner. No Wi-Fi, HTTP server, or LED control yet.
- TODO: define the target's HTTP endpoints (paths, methods, response format).
  `firmware/bridge-board/components/target_client` depends on them.
- TODO: LED GPIO pin (from `hardware/docs/`).
- **TODO (team decision): hardcoded credentials vs. R2.** R2 says credentials
  never appear in the repository. This device is *supposed* to have hardcoded
  credentials. Decide whether they are committed as a documented, lab-only
  exception, or injected at build time through `menuconfig` (git-ignored
  `sdkconfig`) so the binary is still "hardcoded" but the repo stays clean.
  The second option is recommended.
