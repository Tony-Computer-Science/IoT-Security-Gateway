#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/test/test_device_server.py
python3 web/scripts/build.py
