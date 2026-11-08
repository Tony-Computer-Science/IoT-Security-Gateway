#!/usr/bin/env python3
"""Compile the exact firmware parser with its pinned managed cJSON dependency."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "firmware/target-device/main"
CJSON = ROOT / "firmware/target-device/managed_components/espressif__cjson/cJSON"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitizers", choices=("address,undefined", "undefined"),
                        default="address,undefined")
    args = parser.parse_args()
    if not (CJSON / "cJSON.c").is_file():
        raise SystemExit("Build firmware/target-device first to install its pinned cJSON dependency")
    with tempfile.TemporaryDirectory(prefix="target-protocol-") as directory:
        executable = Path(directory) / "target-tests"
        cjson_object = Path(directory) / "cjson.o"
        flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=" + args.sanitizers, "-fno-omit-frame-pointer"]
        # Apple's SDK deprecates sprintf used by the pinned upstream cJSON.
        # Limit the exception to that dependency; project code stays strict.
        vendor_flags = ["-Wno-deprecated-declarations"] if sys.platform == "darwin" else []
        subprocess.run([os.environ.get("CC", "cc"), *flags, *vendor_flags,
                        "-I", str(CJSON), "-c", str(CJSON / "cJSON.c"),
                        "-o", str(cjson_object)], check=True)
        subprocess.run([
            os.environ.get("CC", "cc"), *flags, "-I", str(MAIN),
            "-I", str(CJSON), str(MAIN / "target_protocol.c"), str(cjson_object),
            str(ROOT / "tools/test/test_target_protocol.c"), "-lm", "-o", str(executable)
        ], check=True)
        subprocess.run([str(executable)], check=True, timeout=30)


if __name__ == "__main__":
    main()
