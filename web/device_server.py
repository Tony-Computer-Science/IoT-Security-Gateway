#!/usr/bin/env python3
"""Local lab UI/API backed by a real, outbound-polling Board C.

Browser authentication remains required. The device channel is intentionally
unauthenticated plain HTTP, matching the evaluation target's lab-only design.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import secrets
import sys
import threading
import time
from http.server import ThreadingHTTPServer
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from web.mock.server import ACTIONS, ApiError, FIXTURES, Handler, MockGateway

DEVICE_ID = re.compile(r"[A-Za-z0-9_-]{1,32}\Z")
BOOT_ID = re.compile(r"[0-9a-f]{16}\Z")
COMMAND_ID = re.compile(r"[0-9a-f]{32}\Z")


class DeviceGateway(MockGateway):
    """One real target, one in-flight command, and a bounded confirmation wait."""

    def __init__(self, fixtures, password, device_id="iot-c6", *, clock=time.monotonic,
                 stale_s=8, command_timeout_s=5):
        super().__init__(fixtures, password)
        self.changed = threading.Condition(self.lock)
        self.device_id = device_id
        self.clock = clock
        self.stale_s = stale_s
        self.command_timeout_s = command_timeout_s
        self.last_seen = None
        self.device_boot = None
        self.pending = None
        self.link_ok = self.target_reachable = False

    def _refresh(self):
        fresh = self.last_seen is not None and self.clock() - self.last_seen <= self.stale_s
        if self.target_reachable != fresh:
            self._log("target", "allowed" if fresh else "alert",
                      "device channel available" if fresh else "device heartbeat expired")
        self.link_ok = self.target_reachable = fresh
        return fresh

    def status(self, auth):
        with self.lock:
            self._require_session(auth)
            self._refresh()
            return 200, {"output": self.output, "target_reachable": self.target_reachable,
                         "link_ok": self.link_ok, "uptime_s": self.uptime(), "seq": self.seq}

    def command(self, auth, body):
        with self.changed:
            self._require_session(auth)
            if (not isinstance(body, dict) or set(body) != {"action"}
                    or not isinstance(body["action"], str) or body["action"] not in ACTIONS):
                raise ApiError(400, "invalid_action")
            now = time.monotonic()
            rate = self.fx["command_rate_limit"]
            while self.command_times and now - self.command_times[0] >= rate["window_s"]:
                self.command_times.popleft()
            if len(self.command_times) >= rate["max"]:
                raise ApiError(429, "rate_limited", retry_after=max(
                    1, math.ceil(rate["window_s"] - (now - self.command_times[0]))))
            if self.pending is not None:
                raise ApiError(429, "rate_limited", retry_after=1)
            if not self._refresh():
                raise ApiError(502, "target_unreachable")
            self.command_times.append(now)
            pending = {"id": secrets.token_hex(16), "action": body["action"],
                       "boot": self.device_boot, "delivered": False, "result": None,
                       "output": None}
            self.pending = pending
            deadline = time.monotonic() + self.command_timeout_s
            self.changed.notify_all()
            while pending["result"] is None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                self.changed.wait(remaining)
            self.pending = None
            if pending["result"] is True:
                self._log("command", "allowed", pending["action"] + " confirmed by device")
                return 200, {"result": "ok", "output": pending["output"]}
            # Delivered commands may have changed the LED even if the ACK was
            # lost. Do not represent that uncertainty as an unchanged output.
            self._log("target", "alert", "device command confirmation unavailable")
            if pending["delivered"]:
                raise ApiError(504, "command_unconfirmed")
            raise ApiError(502, "target_unreachable")

    def poll(self, body):
        required = {"device_id", "boot_id", "output", "ack_id", "ack_ok", "uptime_s"}
        if (not isinstance(body, dict) or set(body) != required
                or not isinstance(body["device_id"], str)
                or not DEVICE_ID.fullmatch(body["device_id"])
                or not isinstance(body["boot_id"], str) or not BOOT_ID.fullmatch(body["boot_id"])
                or not isinstance(body["output"], str) or body["output"] not in ("on", "off")
                or type(body["ack_ok"]) is not bool
                or (body["ack_id"] is not None and
                    (not isinstance(body["ack_id"], str) or not COMMAND_ID.fullmatch(body["ack_id"])))
                or type(body["uptime_s"]) is not int or not 0 <= body["uptime_s"] <= 0xffffffff):
            raise ApiError(400, "invalid_request")
        if body["device_id"] != self.device_id:
            raise ApiError(403, "unknown_device")
        with self.changed:
            if self.device_boot is not None and self.device_boot != body["boot_id"]:
                self._log("target", "alert", "device restarted; old command invalidated")
                if self.pending is not None:
                    self.pending["result"] = False
            self.device_boot = body["boot_id"]
            self.last_seen = self.clock()
            self.output = body["output"]
            self._refresh()
            pending = self.pending
            if (pending is not None and pending["result"] is None
                    and pending["boot"] == self.device_boot):
                if body["ack_id"] == pending["id"] and pending["delivered"]:
                    pending["result"] = body["ack_ok"] and body["output"] == ACTIONS[pending["action"]]
                    pending["output"] = body["output"]
                if pending["result"] is None:
                    pending["delivered"] = True
                    self.changed.notify_all()
                    return 200, {"command": {"id": pending["id"], "action": pending["action"]}}
            self.changed.notify_all()
            return 200, {"command": None}


class DeviceHandler(Handler):
    server_version = "gateway-device-ui/0.2.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(6)

    def dispatch(self, method):
        path = urlsplit(self.path)
        if path.path.startswith("/__mock/"):
            self.send_json(404, {"error": "not_found"})
            return
        if path.path.startswith("/device/"):
            try:
                if path.path != "/device/poll":
                    raise ApiError(404, "not_found")
                if method != "POST":
                    raise ApiError(405, "method_not_allowed")
                if path.query or self.headers.get_content_type() != "application/json":
                    raise ApiError(400, "invalid_request")
                status, body = self.gateway.poll(self.json_body())
            except ApiError as error:
                status, body = error.status, error.body
            self.send_json(status, body)
            return
        super().dispatch(method)

    def log_message(self, fmt, *args):
        if not self.quiet:
            # Never print headers, bodies, query strings or user-supplied IDs.
            sys.stderr.write("  %s %s\n" % (self.command, urlsplit(self.path).path))


class DeviceHTTPServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, *args, **kwargs):
        self.slots = threading.BoundedSemaphore(16)
        super().__init__(*args, **kwargs)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, *args):
        try:
            super().process_request_thread(*args)
        finally:
            self.slots.release()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.78.10")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--device-id", default="iot-c6")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()
    if not DEVICE_ID.fullmatch(args.device_id) or not 1 <= args.port <= 65535:
        parser.error("invalid device ID or port")
    configured_password = os.environ.get("UI_PASSWORD")
    password = configured_password or secrets.token_urlsafe(12)
    DeviceHandler.gateway = DeviceGateway(FIXTURES, password, args.device_id)
    DeviceHandler.quiet = args.quiet
    with DeviceHTTPServer((args.host, args.port), DeviceHandler) as server:
        print("IoT device UI: http://%s:%d/" % (args.host, args.port))
        print("Username: %s" % FIXTURES["username"])
        print("Password: %s" % ("(from UI_PASSWORD)" if configured_password else password))
        print("Waiting for device %s; lab-only unauthenticated HTTP device channel" % args.device_id)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
