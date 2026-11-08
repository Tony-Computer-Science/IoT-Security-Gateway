#!/usr/bin/env bash
# Legacy asset packer from the former onboard-hosting design.
# The fixed-IP PC serves the UI/API; this image is unused by that service.
#
# Usage:  web/scripts/mklittlefs.sh [output.bin]      default: web/littlefs.bin
#
# Requires:
#   - `python3 web/scripts/build.py` run first (fills web/dist/ with *.gz)
#   - mklittlefs on PATH. macOS: `brew install mklittlefs` (tested with 4.1.0)
#
# The partition name and size are read from firmware/station-board/partitions.csv
# so the image always matches the partition table.
#
# Filesystem geometry, and where each value comes from (esp_littlefs 1.23.1,
# the joltwallet/littlefs component; verify against the version in use):
#
#   BLOCK_SIZE=4096  esp_littlefs mounts flash partitions with a fixed 4096 byte
#                    block: src/esp_littlefs.c, `#define CONFIG_LITTLEFS_BLOCK_SIZE
#                    4096 /* ESP32 can only operate at 4kb */`. Its own CMake image
#                    builder (project_include.cmake) also passes --block-size=4096.
#                    This is the one value that MUST match: it is stored in the
#                    image's superblock and the firmware rejects a mismatch.
#
#   PAGE_SIZE=256    Kconfig CONFIG_LITTLEFS_PAGE_SIZE default (256). Not stored
#                    in the image. mklittlefs 4.1.0 parses -p but does not use it
#                    (its read/program size is fixed at 64 bytes in main.cpp);
#                    it is passed for older mklittlefs versions that do use it.
#                    Any value that is a multiple of 64 and divides 4096 is safe.
#
#   Block count      Not passed: mklittlefs derives it from -s (the partition
#                    size), and the firmware autodetects it from the partition
#                    (cfg.block_count = 0), so they always agree.
#
#   Name length      mklittlefs 4.1.0 is built with LFS_NAME_MAX=255 and records
#                    255 in the image. esp_littlefs does not override littlefs's
#                    default of 255, so the image mounts (littlefs refuses only an
#                    image whose name_max is LARGER than the firmware's).
#                    CONFIG_LITTLEFS_OBJ_NAME_LEN (64) still limits path length in
#                    the firmware, so keep web file names under 64 characters.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIST_DIR="$REPO_ROOT/web/dist"
PARTITIONS="$REPO_ROOT/firmware/station-board/partitions.csv"
PARTITION_NAME="www"
OUTPUT="${1:-$REPO_ROOT/web/littlefs.bin}"

BLOCK_SIZE=4096
PAGE_SIZE=256

if ! command -v mklittlefs >/dev/null 2>&1; then
  echo "error: mklittlefs not found on PATH" >&2
  echo "       macOS: brew install mklittlefs" >&2
  echo "       other: https://github.com/earlephilhower/mklittlefs/releases" >&2
  exit 1
fi

if [ ! -d "$DIST_DIR" ] || [ -z "$(ls -A "$DIST_DIR")" ]; then
  echo "error: $DIST_DIR is empty; run: python3 web/scripts/build.py" >&2
  exit 1
fi

# Only gzipped assets belong in this legacy image. `build.py --keep-min` leaves
# uncompressed copies in dist/ for debugging; refuse to pack those.
EXTRA="$(find "$DIST_DIR" -type f ! -name '*.gz' | sed "s|^$DIST_DIR/||")"
if [ -n "$EXTRA" ]; then
  echo "error: web/dist contains files that are not .gz:" >&2
  echo "$EXTRA" | sed 's/^/         /' >&2
  echo "       rerun: python3 web/scripts/build.py   (without --keep-min)" >&2
  exit 1
fi

# Find the size column of the "www" row (comments and blank lines skipped).
SIZE_FIELD="$(awk -F',' -v name="$PARTITION_NAME" '
  /^[[:space:]]*#/ { next }
  { gsub(/[[:space:]]/, "", $1) }
  $1 == name { gsub(/[[:space:]]/, "", $5); print $5; exit }
' "$PARTITIONS")"

if [ -z "$SIZE_FIELD" ]; then
  echo "error: no '$PARTITION_NAME' partition in $PARTITIONS" >&2
  exit 1
fi

# Convert 0x.. / NNNK / NNNM to bytes.
case "$SIZE_FIELD" in
  0x*|0X*) SIZE_BYTES=$((SIZE_FIELD)) ;;
  *[Kk])   SIZE_BYTES=$(( ${SIZE_FIELD%[Kk]} * 1024 )) ;;
  *[Mm])   SIZE_BYTES=$(( ${SIZE_FIELD%[Mm]} * 1024 * 1024 )) ;;
  *)       SIZE_BYTES=$((SIZE_FIELD)) ;;
esac

echo "Packing $DIST_DIR -> $OUTPUT"
echo "  partition '$PARTITION_NAME': $SIZE_BYTES bytes, block $BLOCK_SIZE, page $PAGE_SIZE"

mklittlefs -c "$DIST_DIR" -s "$SIZE_BYTES" -b "$BLOCK_SIZE" -p "$PAGE_SIZE" "$OUTPUT"

echo "Wrote $(wc -c < "$OUTPUT" | tr -d ' ') bytes."
echo "Check it:  python3 web/scripts/verify_littlefs.py $OUTPUT"
echo "Legacy image only; the current UI runs on the fixed-IP PC and needs no asset flashing."
