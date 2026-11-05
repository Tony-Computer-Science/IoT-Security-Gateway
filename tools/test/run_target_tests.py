#!/usr/bin/env python3
"""Compile the exact firmware parser with its pinned managed cJSON dependency."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "firmware/target-device/main"
CJSON = ROOT / "firmware/target-device/managed_components/espressif__cjson/cJSON"


def main():
    if not (CJSON / "cJSON.c").is_file():
        raise SystemExit("Build firmware/target-device first to install its pinned cJSON dependency")
    with tempfile.TemporaryDirectory(prefix="target-protocol-") as directory:
        executable = Path(directory) / "target-tests"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(MAIN),
            "-I", str(CJSON), str(MAIN / "target_protocol.c"), str(CJSON / "cJSON.c"),
            str(ROOT / "tools/test/test_target_protocol.c"), "-lm", "-o", str(executable)
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
