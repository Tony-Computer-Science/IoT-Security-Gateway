#!/usr/bin/env python3
"""Compile and run the actual firmware codec/policy under ASan and UBSan."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / "firmware/common/gateway_protocol"
PORTABLE_SOURCES = (
    "gateway_frame.c",
    "gateway_session.c",
    "gateway_status.c",
    "gateway_policy.c",
    "gateway_access.c",
    "gateway_uart.c",
)


def main():
    """Use the firmware sources directly; leave no build output in the tree."""
    with tempfile.TemporaryDirectory(prefix="gateway-tests-") as directory:
        executable = Path(directory) / "test_gateway"
        command = [
            os.environ.get("CC", "cc"),
            "-std=c11", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(COMPONENT / "include"),
            *(str(COMPONENT / name) for name in PORTABLE_SOURCES),
            str(ROOT / "tools/test/test_gateway_protocol.c"),
            str(ROOT / "tools/test/test_gateway_access.c"),
            str(ROOT / "tools/test/test_gateway_uart.c"),
            "-o", str(executable),
        ]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
