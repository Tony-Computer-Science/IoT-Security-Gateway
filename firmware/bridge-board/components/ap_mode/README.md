# ap_mode

Hosts the private Wi-Fi access point that only the target device joins. The AP password is withheld from ordinary users.

**Requirements:** R1

## Current owner (provisional)

Zachariah Grande. Any member may work here; the current owner reviews. Reassign by
updating this section and `.github/CODEOWNERS` in the same PR.

## Status

Stub. `ap_mode_init()` returns `ESP_ERR_NOT_SUPPORTED`. The public API is in
`include/ap_mode.h`; expand it as the design firms up.

## Notes

- SSID and password come from `menuconfig` (git-ignored `sdkconfig`), never `sdkconfig.defaults`.
- No IP forwarding, bridging, or NAT, ever.

## Build

Built as part of the `bridge-board` ESP-IDF project (`idf.py build` in
`firmware/bridge-board/`). If this component starts using other ESP-IDF components,
list them in `REQUIRES` / `PRIV_REQUIRES` in `CMakeLists.txt`.
