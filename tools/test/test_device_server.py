#!/usr/bin/env python3
"""Exercise the real PC HTTP server against a protocol-level device actor."""
import copy
import json
from pathlib import Path
import sys
import threading
import time
import unittest
from urllib.error import HTTPError
from urllib.request import ProxyHandler, Request, build_opener

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from web.device_server import DeviceGateway, DeviceHandler, DeviceHTTPServer
from web.mock.server import ACTIONS, ApiError, FIXTURES, MockGateway


def exchange(base, method, path, body=None, token=None):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    data = None if body is None else json.dumps(body).encode()
    request = Request(base + path, data=data, headers=headers, method=method)
    try:
        response = build_opener(ProxyHandler({})).open(request, timeout=3)
    except HTTPError as error:
        response = error
    with response:
        raw = response.read()
        return response.status, json.loads(raw) if raw else None


def report(**changes):
    body = {"device_id": "iot-c6", "boot_id": "0000000000000001", "output": "off",
            "ack_id": None, "ack_ok": True, "uptime_s": 1}
    body.update(changes)
    return body


class DeviceActor:
    """A stand-in, not firmware: uses real HTTP to poll and acknowledge LEDs."""

    def __init__(self, base):
        self.base = base
        self.message = report()
        self.stop_event = threading.Event()
        self.ready = threading.Event()
        self.errors = []
        self.applied = []
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self):
        self.thread.start()
        if not self.ready.wait(2):
            raise RuntimeError("device actor did not register")
        return self

    def run(self):
        while not self.stop_event.is_set():
            try:
                status, body = exchange(self.base, "POST", "/device/poll", self.message)
                if status != 200:
                    raise RuntimeError("poll status %d" % status)
                self.ready.set()
                command = body["command"]
                if command and command["id"] != self.message["ack_id"]:
                    self.message["output"] = ACTIONS[command["action"]]
                    self.message["ack_id"] = command["id"]
                    self.applied.append(command["id"])
            except Exception as error:
                self.errors.append(str(error))
                self.stop_event.set()
            self.stop_event.wait(.02)

    def close(self):
        self.stop_event.set()
        self.thread.join(4)


class DeviceServerTests(unittest.TestCase):
    def setUp(self):
        self.gateway = DeviceGateway(copy.deepcopy(FIXTURES), "test-only-password", command_timeout_s=.2)
        self.handler = type("TestHandler", (DeviceHandler,), {"gateway": self.gateway, "quiet": True})
        self.server = DeviceHTTPServer(("127.0.0.1", 0), self.handler)
        self.worker = threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval": .01}, daemon=True)
        self.worker.start()
        self.base = "http://127.0.0.1:%d" % self.server.server_port
        status, body = exchange(self.base, "POST", "/api/login",
                                {"username": "operator", "password": "test-only-password"})
        self.assertEqual(status, 200)
        self.token = body["token"]

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.worker.join(2)

    def status(self):
        return exchange(self.base, "GET", "/api/status", token=self.token)

    def begin_command(self, action="led_on"):
        result = []
        thread = threading.Thread(target=lambda: result.append(exchange(
            self.base, "POST", "/api/command", {"action": action}, self.token)))
        thread.start()
        with self.gateway.changed:
            self.assertTrue(self.gateway.changed.wait_for(lambda: self.gateway.pending is not None, 1))
        return thread, result

    def test_actual_http_control_and_browser_session(self):
        self.assertFalse(self.status()[1]["target_reachable"])
        actor = DeviceActor(self.base).start()
        try:
            for action, output in (("led_on", "on"), ("led_off", "off")):
                code, body = exchange(self.base, "POST", "/api/command", {"action": action}, self.token)
                self.assertEqual((code, body), (200, {"result": "ok", "output": output}))
                self.assertEqual(self.status()[1]["output"], output)
            self.assertEqual(len(set(actor.applied)), 2)
            code, body = exchange(self.base, "GET", "/api/events", token=self.token)
            self.assertEqual(code, 200)
            self.assertEqual(sum(e["type"] == "command" for e in body["events"]), 2)
            self.assertNotIn(self.token, json.dumps(body))
            self.assertEqual(exchange(self.base, "POST", "/api/logout", token=self.token)[0], 204)
            self.assertEqual(self.status()[0], 401)
            self.assertFalse(actor.errors)
        finally:
            actor.close()

    def test_no_delivery_timeout_cannot_later_replay(self):
        exchange(self.base, "POST", "/device/poll", report())
        code, body = exchange(self.base, "POST", "/api/command", {"action": "led_on"}, self.token)
        self.assertEqual((code, body["error"]), (502, "target_unreachable"))
        self.assertEqual(exchange(self.base, "POST", "/device/poll", report())[1], {"command": None})
        self.assertEqual(self.status()[1]["output"], "off")

    def test_delivery_without_ack_is_unknown_and_late_report_reconciles(self):
        exchange(self.base, "POST", "/device/poll", report())
        thread, result = self.begin_command()
        command = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        again = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        self.assertEqual(command, again)  # lost response retries keep the same ID
        thread.join(2)
        self.assertEqual(result[0], (504, {"error": "command_unconfirmed"}))
        late = report(output="on", ack_id=command["id"])
        self.assertEqual(exchange(self.base, "POST", "/device/poll", late)[1], {"command": None})
        self.assertEqual(self.status()[1]["output"], "on")

    def test_restart_invalidates_old_command(self):
        exchange(self.base, "POST", "/device/poll", report())
        thread, result = self.begin_command()
        command = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        restarted = report(boot_id="0000000000000002", ack_id=command["id"], ack_ok=True)
        self.assertIsNone(exchange(self.base, "POST", "/device/poll", restarted)[1]["command"])
        thread.join(2)
        self.assertEqual(result[0][0], 504)
        self.assertEqual(self.status()[1]["output"], "off")

    def test_wrong_ack_or_wrong_output_never_produces_success(self):
        exchange(self.base, "POST", "/device/poll", report())
        thread, result = self.begin_command()
        command = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        exchange(self.base, "POST", "/device/poll", report(ack_id="f" * 32))
        self.assertTrue(thread.is_alive())
        exchange(self.base, "POST", "/device/poll", report(ack_id=command["id"], output="off"))
        thread.join(2)
        self.assertEqual(result[0][0], 504)

    def test_one_inflight_command_and_no_session_bypass(self):
        exchange(self.base, "POST", "/device/poll", report())
        thread, result = self.begin_command()
        self.assertEqual(exchange(self.base, "POST", "/api/command", {"action": "led_off"})[0], 401)
        self.assertEqual(exchange(self.base, "POST", "/api/command", {"action": "led_off"}, self.token)[0], 429)
        command = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        exchange(self.base, "POST", "/device/poll", report(ack_id=command["id"], output="on"))
        thread.join(2)
        self.assertEqual(result[0][0], 200)

    def test_restart_of_pc_changes_command_ids(self):
        self.gateway.poll(report())
        thread, result = self.begin_command()
        first = self.gateway.poll(report())[1]["command"]["id"]
        self.gateway.poll(report(output="on", ack_id=first))
        thread.join(2)
        replacement = DeviceGateway(copy.deepcopy(FIXTURES), "test-only-password", command_timeout_s=.2)
        self.handler.gateway = self.gateway = replacement
        _, auth = exchange(self.base, "POST", "/api/login", {"username": "operator", "password": "test-only-password"})
        self.token = auth["token"]
        replacement.poll(report(output="on", ack_id=first))
        thread, result = self.begin_command("led_off")
        second = replacement.poll(report(output="on", ack_id=first))[1]["command"]["id"]
        self.assertNotEqual(first, second)
        replacement.poll(report(output="off", ack_id=second))
        thread.join(2)
        self.assertEqual(result[0][0], 200)

    def test_heartbeat_expiry_and_recovery(self):
        now = [10.0]
        self.gateway.clock = lambda: now[0]
        self.gateway.poll(report())
        self.assertTrue(self.status()[1]["target_reachable"])
        now[0] += 9
        self.assertFalse(self.status()[1]["target_reachable"])
        self.assertEqual(exchange(self.base, "POST", "/api/command", {"action": "led_on"}, self.token)[0], 502)
        self.gateway.poll(report())
        self.assertTrue(self.status()[1]["target_reachable"])

    def test_invalid_device_messages_and_actions(self):
        for changes, code in (({"device_id": "another"}, 403), ({"output": []}, 400),
                              ({"uptime_s": True}, 400), ({"uptime_s": -1}, 400),
                              ({"boot_id": "broken"}, 400), ({"ack_id": "bad"}, 400),
                              ({"ack_ok": 1}, 400), ({"extra": 1}, 400)):
            self.assertEqual(exchange(self.base, "POST", "/device/poll", report(**changes))[0], code)
        for action in (None, {}, [], "reboot", 5):
            self.assertEqual(exchange(self.base, "POST", "/api/command", {"action": action}, self.token)[0], 400)
        self.assertEqual(exchange(self.base, "POST", "/device/poll", {"data": "a" * 1100})[0], 413)
        self.assertEqual(exchange(self.base, "GET", "/device/poll")[0], 405)
        self.assertEqual(exchange(self.base, "POST", "/device/poll?x=1", report())[0], 400)
        self.assertEqual(exchange(self.base, "GET", "/__mock/state")[0], 404)

    def test_mock_invalid_action_type_is_a_contract_error(self):
        mock = MockGateway(copy.deepcopy(FIXTURES), "test-only-password")
        _, auth = mock.login({"username": "operator", "password": "test-only-password"})
        with self.assertRaises(ApiError) as error:
            mock.command("Bearer " + auth["token"], {"action": []})
        self.assertEqual(error.exception.status, 400)


if __name__ == "__main__":
    unittest.main(verbosity=2)
