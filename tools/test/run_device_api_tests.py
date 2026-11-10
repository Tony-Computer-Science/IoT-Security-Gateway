#!/usr/bin/env python3
"""Run saved Postman assertions against the real PC server and a device actor."""
import copy
import json
from pathlib import Path
import secrets
import shutil
import subprocess
import tempfile
import threading

from test_device_server import DeviceActor, DeviceGateway, DeviceHandler, DeviceHTTPServer, FIXTURES, ROOT


def main():
    cli = shutil.which("postman")
    if not cli:
        raise SystemExit("Postman CLI is unavailable; run test_device_server.py for stdlib HTTP tests")
    password = secrets.token_urlsafe(18)
    handler = type("CollectionHandler", (DeviceHandler,), {
        "gateway": DeviceGateway(copy.deepcopy(FIXTURES), password), "quiet": True})
    with DeviceHTTPServer(("127.0.0.1", 0), handler, local_ui_only=False) as server:
        worker = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": .01}, daemon=True)
        worker.start()
        base = "http://127.0.0.1:%d" % server.server_port
        actor = DeviceActor(base).start()
        try:
            with tempfile.TemporaryDirectory(prefix="device-api-") as directory:
                environment = Path(directory) / "environment.json"
                environment.write_text(json.dumps({"name": "Local device tests", "values": [
                    {"key": "baseUrl", "value": base, "enabled": True},
                    {"key": "uiPassword", "value": password, "enabled": True}]}))
                environment.chmod(0o600)
                subprocess.run([cli, "collection", "run", str(ROOT / "tools/test/device-api.postman_collection.json"),
                                "-e", str(environment), "--no-report-events", "--bail",
                                "--timeout-request", "8000"], check=True)
                if actor.errors:
                    raise RuntimeError("device actor failed")
        finally:
            actor.close()
            server.shutdown()
            worker.join(2)


if __name__ == "__main__":
    main()
