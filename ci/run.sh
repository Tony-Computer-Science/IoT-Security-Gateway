#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

python3 tools/test/test_device_server.py
python3 tools/test/test_device_isolation.py
node --test tools/test/test_web_api.mjs

# The macOS AddressSanitizer runtime hangs before main on the development
# host, so local macOS runs use UBSan only; Linux CI runs both sanitizers.
if [[ "$(uname -s)" == "Darwin" ]]; then
  python3 tools/test/run_target_tests.py --sanitizers undefined
else
  python3 tools/test/run_target_tests.py
fi

python3 web/scripts/build.py
