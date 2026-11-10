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


def raw_post(base, path, data, token=None):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    request = Request(base + path, data=data, headers=headers, method="POST")
    try:
        response = build_opener(ProxyHandler({})).open(request, timeout=3)
    except HTTPError as error:
        response = error
    with response:
        return response.status, json.loads(response.read() or b"null")


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
        self.server = DeviceHTTPServer(("127.0.0.1", 0), self.handler, local_ui_only=False)
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

    def test_confirmation_history_survives_failed_commands_and_device_restart(self):
        self.assertIsNone(self.status()[1]["last_confirmed_command"])
        actor = DeviceActor(self.base).start()
        try:
            code, _ = exchange(self.base, "POST", "/api/command", {"action": "led_on"}, self.token)
            self.assertEqual(code, 200)
        finally:
            actor.close()
        confirmed = self.status()[1]["last_confirmed_command"]
        self.assertEqual(confirmed["action"], "led_on")
        self.assertEqual(confirmed["output"], "on")
        self.assertIsInstance(confirmed["service_uptime_s"], int)
        _, other = exchange(self.base, "POST", "/api/login",
                            {"username": "operator", "password": "test-only-password"})
        self.assertEqual(exchange(self.base, "GET", "/api/status", token=other["token"])[1]
                         ["last_confirmed_command"], confirmed)
        thread, result = self.begin_command("led_off")
        self.assertEqual(self.status()[1]["last_confirmed_command"], confirmed)
        exchange(self.base, "POST", "/device/poll", report(output="on"))
        thread.join(2)
        self.assertEqual(result[0][0], 504)
        self.assertEqual(self.status()[1]["last_confirmed_command"], confirmed)
        exchange(self.base, "POST", "/device/poll", report(boot_id="0000000000000002", uptime_s=0))
        status = self.status()[1]
        self.assertEqual(status["device_uptime_s"], 0)
        self.assertEqual(status["output"], "off")
        self.assertEqual(status["last_confirmed_command"], confirmed)
        restarted = DeviceGateway(copy.deepcopy(FIXTURES), "test-only-password")
        self.assertIsNone(restarted.last_confirmed_command)

    def test_mock_confirmation_history_and_service_reset(self):
        mock = MockGateway(copy.deepcopy(FIXTURES), "test-only-password")
        _, auth = mock.login({"username": "operator", "password": "test-only-password"})
        token = "Bearer " + auth["token"]
        self.assertIsNone(mock.status(token)[1]["last_confirmed_command"])
        mock.command(token, {"action": "led_on"})
        confirmed = mock.status(token)[1]["last_confirmed_command"]
        self.assertEqual((confirmed["action"], confirmed["output"]), ("led_on", "on"))
        with self.assertRaises(ApiError):
            mock.command(token, {"action": "not_a_command"})
        self.assertEqual(mock.status(token)[1]["last_confirmed_command"], confirmed)
        mock.reset()
        self.assertIsNone(mock.last_confirmed_command)

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
        self.assertIsNone(self.status()[1]["last_confirmed_command"])

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
        # The device answered this command with the wrong state: a known failure.
        self.assertEqual(result[0], (502, {"error": "command_failed", "output": "off"}))

    def test_device_reported_failure_is_distinct_from_unconfirmed(self):
        exchange(self.base, "POST", "/device/poll", report())
        thread, result = self.begin_command()
        command = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        failed = report(ack_id=command["id"], ack_ok=False, output="off")
        self.assertEqual(exchange(self.base, "POST", "/device/poll", failed)[1], {"command": None})
        thread.join(2)
        self.assertEqual(result[0], (502, {"error": "command_failed", "output": "off"}))
        self.assertIsNone(self.status()[1]["last_confirmed_command"])
        events = exchange(self.base, "GET", "/api/events", token=self.token)[1]["events"]
        self.assertTrue(any(e["type"] == "command" and e["outcome"] == "alert"
                            and "device reported output off" in e["detail"] for e in events))
        # The next command is admitted normally; a failure leaves nothing pending.
        thread, result = self.begin_command("led_off")
        command = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        exchange(self.base, "POST", "/device/poll", report(ack_id=command["id"]))
        thread.join(2)
        self.assertEqual(result[0], (200, {"result": "ok", "output": "off"}))

    def test_browser_traffic_cannot_take_device_slots(self):
        server = DeviceHTTPServer(("127.0.0.1", 0), self.handler, total_slots=3, device_slots=2,
                                  local_ui_only=False)
        worker = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": .01}, daemon=True)
        worker.start()
        base = "http://127.0.0.1:%d" % server.server_port
        try:
            exchange(base, "POST", "/device/poll", report())
            result = []
            command = threading.Thread(target=lambda: result.append(exchange(
                base, "POST", "/api/command", {"action": "led_on"}, self.token)))
            command.start()  # holds the only browser slot while awaiting the device
            with self.gateway.changed:
                self.assertTrue(self.gateway.changed.wait_for(lambda: self.gateway.pending is not None, 1))
            self.assertEqual(exchange(base, "GET", "/api/status", token=self.token),
                             (503, {"error": "server_busy", "retry_after": 1}))
            delivered = exchange(base, "POST", "/device/poll", report())
            self.assertEqual(delivered[0], 200)
            exchange(base, "POST", "/device/poll", report(ack_id=delivered[1]["command"]["id"], output="on"))
            command.join(2)
            self.assertEqual(result[0], (200, {"result": "ok", "output": "on"}))
            self.assertEqual(exchange(base, "GET", "/api/status", token=self.token)[0], 200)
        finally:
            server.shutdown()
            server.server_close()
            worker.join(2)
        with self.assertRaises(ValueError):
            DeviceHTTPServer(("127.0.0.1", 0), self.handler, total_slots=2, device_slots=2)

    def events(self):
        return exchange(self.base, "GET", "/api/events?limit=200", token=self.token)[1]["events"]

    def assert_logged_once(self, request, expected_status, type_, outcome, detail):
        """Run one request and require exactly one new event describing it."""
        before = max((e["seq"] for e in self.events()), default=0)
        status = request()
        self.assertEqual(status, expected_status)
        new = [e for e in self.events() if e["seq"] > before]
        self.assertEqual([(e["type"], e["outcome"], e["detail"]) for e in new],
                         [(type_, outcome, detail)])

    def test_every_rejected_command_appears_in_events(self):
        self.gateway.command_timeout_s = 3  # keep the in-flight command alive while probing
        command = lambda body: (lambda: exchange(self.base, "POST", "/api/command", body, self.token)[0])
        # Nothing has reported yet: refused before delivery.
        self.assert_logged_once(command({"action": "led_on"}), 502,
                                "command", "denied", "led_on rejected: device unreachable")
        exchange(self.base, "POST", "/device/poll", report())
        # Client-supplied values are never echoed into the log.
        self.assert_logged_once(command({"action": "<script>reboot"}), 400,
                                "command", "denied", "invalid action rejected")
        self.assert_logged_once(command({"action": "led_on", "extra": 1}), 400,
                                "command", "denied", "invalid action rejected")
        self.assert_logged_once(
            lambda: raw_post(self.base, "/api/command", b"{not json", self.token)[0], 400,
            "command", "denied", "malformed command request rejected")
        self.assert_logged_once(
            lambda: raw_post(self.base, "/api/command", b'{"action":"' + b"x" * 2000 + b'"}', self.token)[0],
            413, "command", "denied", "oversized command request rejected")
        thread, result = self.begin_command()
        self.assert_logged_once(command({"action": "led_off"}), 429,
                                "command", "denied", "led_off rejected: previous command still awaiting device")
        delivered = exchange(self.base, "POST", "/device/poll", report())[1]["command"]
        exchange(self.base, "POST", "/device/poll", report(ack_id=delivered["id"], output="on"))
        thread.join(2)
        self.assertEqual(result[0][0], 200)
        for e in self.events():
            self.assertNotIn("<script>", e["detail"])
            self.assertNotIn(self.token, e["detail"])

    def test_rate_limited_commands_appear_in_events(self):
        actor = DeviceActor(self.base).start()
        try:
            limit = self.gateway.fx["command_rate_limit"]["max"]
            for i in range(limit):
                code = exchange(self.base, "POST", "/api/command",
                                {"action": "led_on" if i % 2 else "led_off"}, self.token)[0]
                self.assertEqual(code, 200)
            self.assert_logged_once(
                lambda: exchange(self.base, "POST", "/api/command", {"action": "led_on"}, self.token)[0],
                429, "rate_limit", "alert", "command rate limit exceeded")
        finally:
            actor.close()

    def test_delivery_failures_name_the_command(self):
        exchange(self.base, "POST", "/device/poll", report())
        thread, result = self.begin_command()
        before = max(e["seq"] for e in self.events())
        thread.join(2)  # never delivered: the device stops polling
        self.assertEqual(result[0][0], 502)
        self.assertIn(("command", "alert", "led_on not delivered: device stopped polling"),
                      [(e["type"], e["outcome"], e["detail"]) for e in self.events() if e["seq"] > before])
        exchange(self.base, "POST", "/device/poll", report())
        thread, result = self.begin_command("led_off")
        exchange(self.base, "POST", "/device/poll", report())  # delivered, never acknowledged
        thread.join(2)
        self.assertEqual(result[0][0], 504)
        self.assertIn(("command", "alert", "led_off unconfirmed: no device acknowledgement"),
                      [(e["type"], e["outcome"], e["detail"]) for e in self.events()])

    def test_malformed_login_requests_appear_in_events(self):
        login = lambda data: (lambda: raw_post(self.base, "/api/login", data)[0])
        self.assert_logged_once(login(b"{broken"), 400, "auth", "denied", "malformed login request rejected")
        self.assert_logged_once(login(b'{"username": 1, "password": "x"}'), 400,
                                "auth", "denied", "malformed login request rejected")

    def test_declared_api_versions_agree(self):
        import re
        from web.mock.server import API_VERSION
        contract = re.search(r"\*\*Version: ([0-9.]+)\*\*", (ROOT / "docs/api-contract.md").read_text())
        client = re.search(r"export const API_VERSION = '([0-9.]+)';", (ROOT / "web/src/api.js").read_text())
        self.assertEqual({contract.group(1), client.group(1), API_VERSION}, {API_VERSION})
        self.assertEqual(DeviceHandler.server_version, "gateway-device-ui/" + API_VERSION)

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

    def test_event_view_detects_expiry_without_status_poll(self):
        now = [100.0]
        self.gateway.clock = lambda: now[0]
        self.gateway.poll(report())
        now[0] += 9
        status, body = exchange(self.base, "GET", "/api/events?outcome=alert", token=self.token)
        self.assertEqual(status, 200)
        self.assertEqual(body["events"][0]["detail"], "device heartbeat expired")
        count = len(body["events"])
        body = exchange(self.base, "GET", "/api/events?outcome=alert", token=self.token)[1]
        self.assertEqual(len(body["events"]), count)

    def test_unseen_device_is_unknown_not_off(self):
        status, body = self.status()
        self.assertEqual(status, 200)
        self.assertEqual(body["status_source"], "device_http")
        self.assertIsNone(body["output"])
        self.assertIsNone(body["last_seen_s"])
        self.assertIsNone(body["device_uptime_s"])
        self.assertFalse(body["target_reachable"])

    def test_freshness_preserves_last_report_and_restart_resets_uptime(self):
        now = [100.0]
        self.gateway.clock = lambda: now[0]
        self.gateway.poll(report(output="on", uptime_s=42))
        now[0] += 9
        body = self.status()[1]
        self.assertEqual(body["last_seen_s"], 9)
        self.assertEqual(body["device_uptime_s"], 42)
        self.assertEqual(body["output"], "on")
        self.assertFalse(body["target_reachable"])
        self.gateway.poll(report(boot_id="0000000000000002", uptime_s=0))
        body = self.status()[1]
        self.assertEqual(body["last_seen_s"], 0)
        self.assertEqual(body["device_uptime_s"], 0)
        self.assertEqual(body["output"], "off")
        self.assertTrue(body["target_reachable"])

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
