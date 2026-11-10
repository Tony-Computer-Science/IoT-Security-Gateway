#!/usr/bin/env python3
"""Compile the exact firmware parser with its pinned managed cJSON dependency.

Uses firmware/target-device/managed_components when an ESP-IDF build has
installed it. Otherwise (for example in CI) it downloads the same pinned
component archive from the Espressif registry and verifies its hashes.
"""
import argparse
import hashlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "firmware/target-device/main"
CJSON = ROOT / "firmware/target-device/managed_components/espressif__cjson/cJSON"
# espressif/cjson 1.7.19~2, as pinned in firmware/target-device/main/idf_component.yml.
CJSON_ARCHIVE = ("https://components-file.espressif.com/components/espressif/cjson/"
                 "1.7.19~2/espressif__cjson-v1.7.19_2.zip")
CJSON_ARCHIVE_SHA256 = "2e13f18fa612d85eeec4001dee87b246beac64cf019bc28d4c4e2e1910adc7f1"
CJSON_FILE_SHA256 = {
    "cJSON.c": "607e756460fa0de37d20a7a9181f2de29c97bfb7ce5a0e6c2f548243836cd852",
    "cJSON.h": "25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee",
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def verify_cjson(directory):
    for name, expected in CJSON_FILE_SHA256.items():
        if sha256((directory / name).read_bytes()) != expected:
            raise SystemExit("%s does not match pinned cJSON 1.7.19~2" % (directory / name))
    return directory


def fetch_cjson(destination):
    with urllib.request.urlopen(CJSON_ARCHIVE, timeout=60) as response:
        archive = response.read()
    if sha256(archive) != CJSON_ARCHIVE_SHA256:
        raise SystemExit("Downloaded cJSON archive hash mismatch")
    with zipfile.ZipFile(io.BytesIO(archive)) as bundle:
        for name in CJSON_FILE_SHA256:
            (destination / name).write_bytes(bundle.read("cJSON/" + name))
    return verify_cjson(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitizers", choices=("address,undefined", "undefined"),
                        default="address,undefined")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="target-protocol-") as directory:
        if (CJSON / "cJSON.c").is_file():
            cjson = verify_cjson(CJSON)
        else:
            print("managed cJSON not installed; fetching pinned espressif/cjson 1.7.19~2")
            cjson = fetch_cjson(Path(directory))
        executable = Path(directory) / "target-tests"
        cjson_object = Path(directory) / "cjson.o"
        flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=" + args.sanitizers, "-fno-omit-frame-pointer"]
        # Apple's SDK deprecates sprintf used by the pinned upstream cJSON.
        # Limit the exception to that dependency; project code stays strict.
        vendor_flags = ["-Wno-deprecated-declarations"] if sys.platform == "darwin" else []
        subprocess.run([os.environ.get("CC", "cc"), *flags, *vendor_flags,
                        "-I", str(cjson), "-c", str(cjson / "cJSON.c"),
                        "-o", str(cjson_object)], check=True)
        subprocess.run([
            os.environ.get("CC", "cc"), *flags, "-I", str(MAIN),
            "-I", str(cjson), str(MAIN / "target_protocol.c"), str(cjson_object),
            str(ROOT / "tools/test/test_target_protocol.c"), "-lm", "-o", str(executable)
        ], check=True)
        subprocess.run([str(executable)], check=True, timeout=30)
        network_test = Path(directory) / "target-network-tests"
        subprocess.run([os.environ.get("CC", "cc"), *flags, "-I", str(MAIN),
                        str(MAIN / "target_network_policy.c"),
                        str(ROOT / "tools/test/test_target_network.c"),
                        "-o", str(network_test)], check=True)
        subprocess.run([str(network_test)], check=True, timeout=30)


if __name__ == "__main__":
    main()
