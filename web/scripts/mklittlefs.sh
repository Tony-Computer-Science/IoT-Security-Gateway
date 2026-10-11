#!/usr/bin/env bash
# Pack web/dist/ into a LittleFS image for Board A's web-asset partition.
#
# Usage:  web/scripts/mklittlefs.sh [output.bin]
#
# Requires:
#   - `python3 web/scripts/build.py` to have been run (fills web/dist/)
#   - the `mklittlefs` tool on PATH (https://github.com/earlephilhower/mklittlefs)
#
# The partition name and size are read from firmware/station-board/partitions.csv
# so the image always matches the partition table.
#
# TODO(firmware): BLOCK_SIZE and PAGE_SIZE must match the esp_littlefs
#   component's Kconfig (CONFIG_LITTLEFS_BLOCK_SIZE / CONFIG_LITTLEFS_PAGE_SIZE)
#   once that component is added to station-board. 4096 / 256 are the
#   esp_littlefs defaults; confirm before relying on this image.
# TODO(firmware): consider replacing this script with esp_littlefs's CMake
#   helper `littlefs_create_partition_image(<partition> ../../web/dist
#   FLASH_IN_PROJECT)` so `idf.py flash` writes the web assets automatically.

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
  echo "       install from https://github.com/earlephilhower/mklittlefs/releases" >&2
  exit 1
fi

if [ ! -d "$DIST_DIR" ] || [ -z "$(ls -A "$DIST_DIR")" ]; then
  echo "error: $DIST_DIR is empty; run: python3 web/scripts/build.py" >&2
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

echo "Done. Flash with:"
echo "  parttool.py --port <PORT> write_partition --partition-name $PARTITION_NAME --input $OUTPUT"
