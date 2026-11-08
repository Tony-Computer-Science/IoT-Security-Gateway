#!/usr/bin/env python3
"""Verify a legacy LittleFS image of the web assets against web/dist.

The current fixed-IP PC service does not use an asset image or mount LittleFS.

Mounts the image with the geometry esp_littlefs uses on the board (4096-byte
blocks, block count taken from the image size) and checks that it holds
exactly the files in web/dist, byte for byte. Works on images from either
historical image builder. The current firmware does not build or mount www.bin.

Read-only: the image is never formatted or written. (littlefs-python's
default is to format an image that fails to mount; this script does not.)

Needs littlefs-python, pinned to the version esp_littlefs uses to build images:
    python3 -m pip install -r web/scripts/requirements-image.txt

Usage:
    python3 web/scripts/verify_littlefs.py [image.bin]     default: web/littlefs.bin
Exit status is non-zero on any mismatch.
"""

import argparse
import hashlib
import re
import sys
from pathlib import Path

try:
    import littlefs
    from littlefs import UserContext
except ImportError:
    sys.exit("error: littlefs-python is not installed; run:\n"
             "       python3 -m pip install -r web/scripts/requirements-image.txt")

REPO = Path(__file__).resolve().parents[2]
DIST = REPO / "web" / "dist"
PARTITIONS = REPO / "firmware" / "station-board" / "partitions.csv"
PARTITION = "www"
BLOCK_SIZE = 4096  # esp_littlefs: src/esp_littlefs.c CONFIG_LITTLEFS_BLOCK_SIZE


def partition_size(name):
    """Size in bytes of `name` in partitions.csv, or None if not found."""
    for line in PARTITIONS.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        cols = [c.strip() for c in line.split(",")]
        if cols[0] == name and len(cols) > 4:
            m = re.fullmatch(r"(0x[0-9a-fA-F]+|\d+)([KkMm]?)", cols[4])
            if not m:
                return None
            n = int(m.group(1), 0)
            return n * {"": 1, "k": 1024, "m": 1024 * 1024}[m.group(2).lower()]
    return None


def image_files(fs):
    """{path: bytes} for every regular file in the image."""
    out = {}
    for root, _dirs, files in fs.walk("/"):
        for name in files:
            path = (root.rstrip("/") + "/" + name)
            with fs.open(path, "rb") as f:
                out[path] = f.read()
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("image", nargs="?", default=str(REPO / "web" / "littlefs.bin"))
    args = ap.parse_args()

    image = Path(args.image)
    data = image.read_bytes()
    problems = []

    expected_size = partition_size(PARTITION)
    size_note = ""
    if expected_size is not None:
        if len(data) == expected_size:
            size_note = f"(matches partition '{PARTITION}' size)"
        else:
            size_note = f"(partition '{PARTITION}' is {expected_size:,} bytes)"
            problems.append(f"image is {len(data):,} bytes but partition '{PARTITION}' is {expected_size:,}")
    if len(data) % BLOCK_SIZE:
        sys.exit(f"error: image size {len(data)} is not a multiple of {BLOCK_SIZE}")

    ctx = UserContext(len(data))
    ctx.buffer = bytearray(data)
    fs = littlefs.LittleFS(context=ctx, mount=False, block_size=BLOCK_SIZE,
                           block_count=len(data) // BLOCK_SIZE,
                           read_size=128, prog_size=128, cache_size=512,
                           lookahead_size=128, name_max=255)
    try:
        fs.mount()
    except littlefs.LittleFSError as e:
        sys.exit(f"error: image does not mount as LittleFS with {BLOCK_SIZE}-byte blocks: {e}")

    st = fs.fs_stat()
    in_image = image_files(fs)
    used = fs.used_block_count
    fs.unmount()
    if bytes(ctx.buffer) != data:
        problems.append("image bytes changed while verifying (should never happen)")

    in_dist = {"/" + p.relative_to(DIST).as_posix(): p.read_bytes()
               for p in sorted(DIST.rglob("*")) if p.is_file()} if DIST.is_dir() else {}

    dv = st.disk_version
    print(f"Image:      {image}  {len(data):,} bytes {size_note}")
    print(f"Superblock: block size {st.block_size}, block count {st.block_count}, "
          f"name_max {st.name_max}, disk version {dv >> 16}.{dv & 0xFFFF}")
    print(f"Used:       {used} of {st.block_count} blocks ({used * BLOCK_SIZE:,} bytes)")
    print()
    print(f"  {'path':<18} {'image bytes':>12} {'dist bytes':>12}  content")
    for path in sorted(set(in_image) | set(in_dist)):
        a, b = in_image.get(path), in_dist.get(path)
        if a is None:
            verdict = "MISSING from image"
        elif b is None:
            verdict = "EXTRA (not in web/dist)"
        elif a == b:
            verdict = "identical  sha256 " + hashlib.sha256(a).hexdigest()[:12]
        else:
            verdict = "DIFFERENT"
        if not verdict.startswith("identical"):
            problems.append(f"{path}: {verdict}")
        print(f"  {path:<18} {'-' if a is None else format(len(a), ','):>12} "
              f"{'-' if b is None else format(len(b), ','):>12}  {verdict}")

    if st.block_size != BLOCK_SIZE:
        problems.append(f"block size {st.block_size}, firmware needs {BLOCK_SIZE}")
    if not in_dist:
        problems.append("web/dist is empty; run python3 web/scripts/build.py")

    print()
    if problems:
        print("Result: FAIL")
        for p in problems:
            print("  - " + p)
        return 1
    print(f"Result: OK, image holds exactly the {len(in_dist)} files in web/dist, byte for byte")
    return 0


if __name__ == "__main__":
    sys.exit(main())
