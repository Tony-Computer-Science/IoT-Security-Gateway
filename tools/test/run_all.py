"""Runs every case script and summarises the outcome.

    python3 tools/test/run_all.py [gateway-url]

Exits non-zero if any case fails. Credentials come from GATEWAY_USER and
GATEWAY_PASSWORD; pass the URL as the first argument to test real hardware.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CATEGORIES = ("1-authorized-operation", "2-unauthorized-access",
              "3-policy-and-isolation", "4-rate-limits-and-recovery")


def main():
    url = sys.argv[1] if len(sys.argv) > 1 else None
    failures, total = 0, 0
    for category in CATEGORIES:
        folder = os.path.join(HERE, category)
        if not os.path.isdir(folder):
            continue
        print("\n== %s" % category)
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".py") or name.startswith("_"):
                continue
            total += 1
            cmd = [sys.executable, os.path.join(folder, name)] + ([url] if url else [])
            result = subprocess.run(cmd, capture_output=True, text=True)
            print(result.stdout.rstrip())
            if result.returncode != 0:
                failures += 1
                if result.stderr.strip():
                    print("  stderr: %s" % result.stderr.strip()[:300])
    print("\n%d/%d case scripts passed" % (total - failures, total))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
