# web/scripts: web build and packaging tooling

Named `scripts/`, **not** `build/`. ESP-IDF's ignore pattern `build/` matches at
any depth and would hide a `web/build/` directory from git.

| File | What it does |
|---|---|
| `build.py` | Minifies `web/src` (pure Python), gzips each file into `web/dist/*.gz`, prints a size report, and fails if the gzipped total exceeds 60 KB or a forbidden API or external URL is used. `--keep-min` also writes uncompressed copies (debugging only; do not pack those). |
| `mklittlefs.sh` | Packs `web/dist/` into a LittleFS image sized from the `www` row of `firmware/station-board/partitions.csv`. Path A below. |
| `verify_littlefs.py` | Mounts an image (from either path) and checks it holds exactly the files in `web/dist`, byte for byte. Never writes the image. |
| `requirements-image.txt` | `littlefs-python==0.15.0` for the verifier: the same version esp_littlefs uses in path B. Tooling only; nothing here ships to the board. |

Note: `web/src/` deliberately has no README. Every file in `web/src/` is
shipped to the board's flash. See [../README.md](../README.md) instead.

## Current owner (provisional)

Lalit Dhakal. Any member may work here; the current owner reviews.

---

## Two ways to build the image

Both start from the same input: run `python3 web/scripts/build.py` first, so
`web/dist/` holds only the four `*.gz` files. Both produce a LittleFS image
the size of the `www` partition, with 4096-byte blocks, which is the geometry
the firmware's esp_littlefs component mounts (see the comments at the top of
`mklittlefs.sh` for where each value comes from).

| | Path A: standalone `mklittlefs` | Path B: ESP-IDF build (esp_littlefs CMake) |
|---|---|---|
| Who it suits | Web-only changes; anyone without ESP-IDF | Firmware work (Tony); one command builds and flashes everything |
| Tool | `mklittlefs` 4.1.0 | `littlefs-python` 0.15.0, installed automatically by the build |
| Command | `web/scripts/mklittlefs.sh` | `idf.py build` in `firmware/station-board` |
| **File to flash** | **`web/littlefs.bin`** | **`firmware/station-board/build/www.bin`** |
| How it gets flashed | A separate step, see "Flashing the image" | Automatically by `idf.py flash` (on ESP-IDF 5.x, alone with `idf.py www-flash`) |
| Name limit recorded in image | 255 | 64 (`CONFIG_LITTLEFS_OBJ_NAME_LEN`) |
| Status | Run and verified on macOS (output below) | Generator run and verified with the exact arguments; full `idf.py` integration not run (no ESP-IDF on the test machine) |

Both images mount on the board. The different name limits are fine: littlefs
only refuses an image whose limit is *larger* than the firmware's, and
esp_littlefs mounts with littlefs's default of 255. Keep file names under 64
characters either way.

### Path A: standalone mklittlefs

Install once. On macOS:

```sh
brew install mklittlefs          # tested with 4.1.0
```

On Linux or Windows, download a release from
https://github.com/earlephilhower/mklittlefs/releases and put it on `PATH`.

Then:

```sh
python3 web/scripts/build.py
web/scripts/mklittlefs.sh        # -> web/littlefs.bin (git-ignored)
```

### Path B: built by idf.py (proposed, not yet applied)

This needs two small additions to `firmware/station-board`, which is the
station board owner's directory, so they are written here as a proposal for
that owner rather than committed:

1. `firmware/station-board/main/idf_component.yml` (new file) adds the component:

   ```yaml
   dependencies:
     joltwallet/littlefs: "==1.23.1"
   ```

2. `firmware/station-board/CMakeLists.txt`, after the `project(...)` line:

   ```cmake
   # Build the web assets into the "www" partition image and flash it with the app.
   littlefs_create_partition_image(www ../../web/dist FLASH_IN_PROJECT)
   ```

Then `python3 web/scripts/build.py` followed by `idf.py build` writes
`build/www.bin`, and `idf.py flash` writes the app and the web assets together.
Things to know:

- `web/dist` must exist before `idf.py build`, or the image step fails.
- The image is rebuilt on every `idf.py build`.
- The first build creates a Python virtual environment inside `build/` and
  installs `littlefs-python` into it, so it needs internet access once.
- Pin the component version. The geometry notes in `mklittlefs.sh` were
  checked against 1.23.1.

## Verifying an image

```sh
python3 -m pip install -r web/scripts/requirements-image.txt   # once
python3 web/scripts/verify_littlefs.py                          # web/littlefs.bin
python3 web/scripts/verify_littlefs.py firmware/station-board/build/www.bin
```

Output for the current assets (path A, macOS, mklittlefs 4.1.0):

```
Image:      web/littlefs.bin  524,288 bytes (matches partition 'www' size)
Superblock: block size 4096, block count 128, name_max 255, disk version 2.1
Used:       7 of 128 blocks (28,672 bytes)

  path                image bytes   dist bytes  content
  /api.js.gz                1,446        1,446  identical  sha256 5aa571205a4f
  /app.js.gz                4,932        4,932  identical  sha256 b838d6dc4429
  /index.html.gz            1,557        1,557  identical  sha256 c280f19d28ad
  /style.css.gz             3,829        3,829  identical  sha256 d938fcc45c1f

Result: OK, image holds exactly the 4 files in web/dist, byte for byte
```

It exits non-zero if a file is missing, extra, or different, if the image is
not the partition's size, or if it does not mount with 4096-byte blocks.

## Flashing the image

Writing the `www` partition replaces only the web assets. The app, Wi-Fi
settings and other partitions are untouched. You do not need to have built
the image yourself; you need the `.bin` file and the board on USB.

1. **Find the board's serial port.** Plug in the ESP32-S3-DevKitC-1 and run
   `ls /dev/cu.*` (macOS). The port labelled USB on the board shows up as
   `/dev/cu.usbmodem…`; the one labelled UART as `/dev/cu.usbserial-…` or
   `/dev/cu.SLAB_USBtoUART`. Either works. On Linux it is `/dev/ttyACM0` or
   `/dev/ttyUSB0`; on Windows, a `COM` port.

2. **Flash.** Pick one option. Replace `/dev/cu.usbmodem1101` with your port.

   **Option 1, ESP-IDF installed (recommended).** `parttool.py` finds the
   `www` partition by name in the partition table already on the board, so no
   address is needed. The board must already be running the station-board
   firmware with this partition table.

   ```sh
   . $IDF_PATH/export.sh
   python "$IDF_PATH/components/partition_table/parttool.py" \
     --port /dev/cu.usbmodem1101 \
     write_partition --partition-name www --input web/littlefs.bin
   ```

   **Option 2, no ESP-IDF, esptool only.** You must give the partition's flash
   address. For the current `partitions.csv` it is `0x190000`. **This address
   changes if the partition table changes** (see the proposal below); the
   station board owner can confirm it with `idf.py partition-table`.

   ```sh
   python3 -m pip install esptool          # once
   python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem1101 \
     write_flash 0x190000 web/littlefs.bin
   ```

   (esptool 5 prefers the spelling `write-flash`; `write_flash` still works.)

3. **Reset the board** (EN button). The firmware mounts the partition labelled
   `www` at boot.

If the board was built with path B, `idf.py flash` already wrote the image and
none of this is needed.

---

## Proposal for the station board owner: partition sizes for 8 MB flash

**Not applied.** `firmware/station-board/partitions.csv` and `sdkconfig.defaults`
belong to the station board owner. This section is the input for that change.

**What changed.** The schematic confirms the module is ESP32-S3-WROOM-1-N8R2:
8 MB flash and 2 MB quad PSRAM. The current files were written for a 4 MB
lower bound.

### The app partition: 1536K is likely tight

The station board app will contain Wi-Fi, the lwIP network stack, mbedTLS, the
HTTPS server, esp_littlefs, the SPI driver, and our own components and log
strings. These are estimates, not measurements, since none of that code is
written yet:

- An ESP-IDF HTTPS server application with Wi-Fi on ESP32-S3 is typically
  around 0.9 to 1.1 MB.
- esp_littlefs and our own components add tens of KB.
- ESP-IDF builds with debug optimisation by default, which is larger.

A realistic range is 1.0 to 1.3 MB, which would leave only about 15 to 35
percent of a 1536K partition free, and `idf.py build` fails outright once the
app no longer fits. With 8 MB there is no reason to run that close.
**Proposed: 3 MB per app slot.** Measure with `idf.py size` once `net_station`
and `auth` are in, and revisit.

### The www partition: 512K is plenty, keep it

The assets are 11,764 bytes gzipped and occupy 7 of the partition's 128
blocks (28 KB) once stored. Even at the full 60 KB asset budget, the
filesystem would use roughly 20 blocks. **Proposed: keep 512K.**

### Two layouts that fit 8 MB

Offsets are left blank in the CSV as today; ESP-IDF fills them in. The offsets
shown were computed by hand from ESP-IDF's rule (table at `0x8000`, apps
aligned to 64 KB, data to 4 KB); confirm with `idf.py partition-table`.

**A. One app, no OTA** (smallest change from today):

```
# Name,    Type, SubType,  Offset, Size,   Flags
nvs,       data, nvs,      ,       0x6000,
phy_init,  data, phy,      ,       0x1000,
factory,   app,  factory,  ,       3M,
www,       data, littlefs, ,       512K,
```

`factory` at `0x10000`, `www` at `0x310000`. Ends at `0x390000`, leaving about
4.4 MB unused.

**B. Two OTA slots** (if the team wants over-the-air updates; this is the open
`TODO(station-board)` in `partitions.csv`):

```
# Name,    Type, SubType,  Offset, Size,   Flags
nvs,       data, nvs,      ,       0x6000,
otadata,   data, ota,      ,       0x2000,
phy_init,  data, phy,      ,       0x1000,
ota_0,     app,  ota_0,    ,       3M,
ota_1,     app,  ota_1,    ,       3M,
www,       data, littlefs, ,       512K,
```

`ota_0` at `0x20000`, `ota_1` at `0x320000`, `www` at `0x620000`. Ends at
`0x6A0000`, leaving about 1.4 MB unused. This layout does **not** fit 4 MB.

Either way, the `www` address in "Flashing the image", option 2, changes from
`0x190000`. Option 1 and path B find the partition by name and need no change.

### sdkconfig.defaults needs updating

- `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` should become
  `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y`. With 4 MB set, layout B fails to build,
  and the build never uses the upper 4 MB.
- The comment above it ("confirm the module's flash size") can now say N8R2.
- Optional, owner's call: the 2 MB PSRAM is not enabled. `CONFIG_SPIRAM=y`
  (quad mode, the N8R2 type) would give room for mbedTLS buffers if RAM gets
  tight once HTTPS is running.
- The `partitions.csv` comment "the total currently needs a 4 MB flash" will
  be out of date after the change.
