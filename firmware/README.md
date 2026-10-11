# firmware/

ESP-IDF v5.x projects for the three ESP32-S3-DevKitC-1 boards. Each
subdirectory is a separate, independently buildable ESP-IDF project.

| Project | Board | Role |
|---|---|---|
| [station-board/](station-board/) | A | HTTPS gateway: Wi-Fi station, auth, policy, event log, SPI controller, serves web UI |
| [bridge-board/](bridge-board/) | B | SPI peripheral, private AP, fixed-target HTTP client |
| [target-device/](target-device/) | C | **Intentionally vulnerable** device under protection |

## Current owner (provisional)

See each project's README. Station board: Goncalves and Lian. Bridge board and
target device: Grande.

## Build any project

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)
with its environment loaded (`. $IDF_PATH/export.sh`).

```sh
cd firmware/<project>
idf.py set-target esp32s3      # first time only
idf.py build
idf.py -p <PORT> flash monitor
```

`idf.py set-target` and `menuconfig` write `sdkconfig`, which is git-ignored
because it may hold Wi-Fi credentials. Shared defaults go in
`sdkconfig.defaults`, which is tracked and must never contain secrets.

## Conventions

- One ESP-IDF component per area of responsibility under `components/`. Each
  has `CMakeLists.txt`, `include/<name>.h`, `<name>.c`, and a README.
- `main/main.c` only boots and wires components together. Feature logic goes
  in components.
- Source comments describe *what* code does, never *who* owns it. Ownership
  lives in READMEs and `.github/CODEOWNERS`.
