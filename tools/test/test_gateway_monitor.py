#!/usr/bin/env python3
"""Gateway checks and up/down histories, driven by simulated probe results."""
import copy
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from web.device_server import DeviceGateway
from web.gateway_monitor import (GatewayMonitor, History, parse_outside, parse_ping, ping_command,
                                 ping_probe, tcp_probe)
from web.mock.server import FIXTURES


class FakeClock:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


class Probe:
    """Returns queued results; repeats the last one when the queue runs out."""

    def __init__(self, *results):
        self.results = list(results)
        self.calls = 0

    def __call__(self):
        self.calls += 1
        return self.results.pop(0) if len(self.results) > 1 else self.results[0]


UP, DOWN = (True, 3.2), (False, None)


def report(**changes):
    body = {"device_id": "iot-c6", "boot_id": "0000000000000001", "output": "off",
            "ack_id": None, "ack_ok": True, "uptime_s": 1}
    body.update(changes)
    return body


class GatewayMonitorTests(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.gateway = DeviceGateway(copy.deepcopy(FIXTURES), "pw", clock=self.clock)
        self.board_a = Probe(UP)
        self.path = Probe(UP)
        self.monitor = GatewayMonitor(self.gateway, self.board_a, self.path,
                                      gateway_ip="192.168.78.1", outside="1.1.1.1:443",
                                      board_a_interval_s=2, path_interval_s=5)
        self.gateway.monitor = self.monitor

    def advance(self, seconds, step=1):
        for _ in range(int(seconds / step)):
            self.clock.t += step
            self.monitor.tick()

    def gateway_events(self):
        return [(e["outcome"], e["detail"]) for e in self.gateway.events if e["type"] == "gateway"]

    def snapshot(self):
        with self.gateway.lock:
            return self.monitor.snapshot()

    def test_starts_checking_then_reports_forwarding(self):
        self.assertEqual(self.snapshot()["diagnosis"], "checking")
        self.assertEqual(self.snapshot()["board_a"]["state"], "unknown")
        self.monitor.tick()
        snap = self.snapshot()
        self.assertEqual(snap["diagnosis"], "forwarding")
        self.assertEqual(snap["board_a"]["latency_ms"], 3.2)
        self.assertEqual(snap["board_a"]["target"], "192.168.78.1")
        self.assertEqual(snap["gateway_path"]["target"], "1.1.1.1:443")
        self.assertEqual(self.gateway_events(), [
            ("allowed", "Board A answering ping"),
            ("allowed", "traffic passing through the gateway")])

    def test_one_lost_ping_is_not_an_outage(self):
        self.monitor.tick()
        self.board_a.results = [DOWN, UP]
        self.advance(2)
        self.assertEqual(self.snapshot()["board_a"]["state"], "up")
        self.advance(2)
        self.assertEqual(len(self.gateway_events()), 2)

    def test_board_a_outage_is_logged_once_and_recovers(self):
        self.monitor.tick()
        self.board_a.results = [DOWN]
        self.path.results = [DOWN]
        self.advance(20)
        snap = self.snapshot()
        self.assertEqual(snap["diagnosis"], "board_a_down")
        self.assertIsNone(snap["board_a"]["latency_ms"])
        self.assertEqual(self.gateway_events()[2:], [
            ("alert", "Board A not answering ping"),
            ("alert", "no traffic through the gateway: SPI/UART link or Board B upstream down")])
        self.board_a.results = [UP]
        self.path.results = [UP]
        self.advance(6)
        self.assertEqual(self.snapshot()["diagnosis"], "forwarding")
        self.assertEqual(self.gateway_events()[-2:], [
            ("allowed", "Board A answering ping"),
            ("allowed", "traffic passing through the gateway")])

    def test_path_down_with_board_a_up_names_link_or_upstream(self):
        self.monitor.tick()
        self.path.results = [DOWN]
        self.advance(12)
        self.assertEqual(self.snapshot()["diagnosis"], "link_or_upstream_down")
        self.assertEqual(self.snapshot()["board_a"]["state"], "up")

    def test_checks_run_on_their_own_intervals(self):
        self.advance(10)
        self.assertEqual(self.board_a.calls, 5)   # t = 0 (first tick at 1), 2, 4, ... within 10 s
        self.assertEqual(self.path.calls, 2)

    def test_history_keeps_the_window_and_the_state_entering_it(self):
        self.monitor.tick()                       # up at t=0
        self.path.results = [DOWN]
        self.advance(10)                          # down at t=10
        self.path.results = [UP]
        self.advance(400)                         # up again at t=15, then 400 s pass
        history = self.snapshot()["gateway_path"]["history"]
        self.assertEqual(len(history), 1)         # older transitions slid out of the window
        self.assertEqual(history[0]["state"], "up")
        self.assertLessEqual(history[0]["t"], self.snapshot()["now_s"] - 300)

    def test_device_history_follows_heartbeat(self):
        self.monitor.tick()
        self.assertEqual(self.snapshot()["device"]["state"], "unknown")
        self.gateway.poll(report())
        self.assertEqual(self.snapshot()["device"]["state"], "up")
        self.advance(10)                          # 8 s heartbeat timeout
        snap = self.snapshot()
        self.assertEqual(snap["device"]["state"], "down")
        self.assertEqual([p["state"] for p in snap["device"]["history"]], ["unknown", "up", "down"])
        # The monitor's tick expired the device without any browser polling.
        self.assertIn("device heartbeat expired", [e["detail"] for e in self.gateway.events])

    def test_disabled_monitor_still_reports_device_history(self):
        gateway = DeviceGateway(copy.deepcopy(FIXTURES), "pw", clock=self.clock)
        with gateway.lock:
            snap = gateway.monitor.snapshot()
        self.assertFalse(snap["enabled"])
        self.assertEqual(snap["diagnosis"], "not_monitored")
        self.assertEqual(snap["board_a"]["state"], "off")
        self.assertEqual(snap["device"]["state"], "unknown")

    def test_status_carries_monitoring(self):
        _, auth = self.gateway.login({"username": "operator", "password": "pw"})
        self.monitor.tick()
        _, status = self.gateway.status("Bearer " + auth["token"])
        self.assertEqual(status["monitoring"]["diagnosis"], "forwarding")
        self.assertEqual(status["monitoring"]["window_s"], 300)


class ProbeTests(unittest.TestCase):
    def test_ping_commands_per_platform(self):
        self.assertEqual(ping_command("192.168.78.1", 1, "darwin"),
                         ["ping", "-n", "-c", "1", "-W", "1000", "192.168.78.1"])
        self.assertEqual(ping_command("192.168.78.1", 1, "linux"),
                         ["ping", "-n", "-c", "1", "-W", "1", "192.168.78.1"])
        self.assertEqual(ping_command("192.168.78.1", 1, "win32"),
                         ["ping", "-n", "1", "-w", "1000", "192.168.78.1"])

    def test_ping_output_parsing(self):
        mac = b"64 bytes from 192.168.78.1: icmp_seq=0 ttl=255 time=4.731 ms"
        win = b"Reply from 192.168.78.1: bytes=32 time<1ms TTL=255"
        win_unreachable = b"Reply from 192.168.78.10: Destination host unreachable."
        self.assertEqual(parse_ping(0, mac), (True, 4.731))
        self.assertEqual(parse_ping(0, win), (True, 1.0))
        self.assertEqual(parse_ping(0, win_unreachable), (False, None))
        self.assertEqual(parse_ping(2, b""), (False, None))

    def test_ping_probe_rejects_non_ip_targets_and_survives_errors(self):
        with self.assertRaises(ValueError):
            ping_probe("192.168.78.1; rm -rf /")

        def missing_ping(*args, **kwargs):
            raise FileNotFoundError("ping")
        self.assertEqual(ping_probe("192.168.78.1", run=missing_ping), (False, None))

        def slow_ping(*args, **kwargs):
            raise subprocess.TimeoutExpired("ping", 3)
        self.assertEqual(ping_probe("192.168.78.1", run=slow_ping), (False, None))

    def test_tcp_probe(self):
        class Conn:
            def __enter__(self):
                return self

            def __exit__(self, *exc):
                return False
        times = iter([10.0, 10.025])
        ok = tcp_probe("1.1.1.1", 443, connect=lambda addr, timeout: Conn(), clock=lambda: next(times))
        self.assertEqual((ok[0], round(ok[1], 1)), (True, 25.0))

        def refused(addr, timeout):
            raise ConnectionRefusedError()
        self.assertEqual(tcp_probe("1.1.1.1", 443, connect=refused), (False, None))

    def test_outside_must_be_ipv4_and_port(self):
        self.assertEqual(parse_outside("1.1.1.1:443"), ("1.1.1.1", 443))
        for bad in ("example.com:443", "1.1.1.1", "1.1.1.1:0", "1.1.1.1:70000"):
            with self.assertRaises(ValueError):
                parse_outside(bad)

    def test_history_ignores_repeats_and_rejects_unknown_states(self):
        history = History(300)
        self.assertFalse(history.record(1, "unknown"))
        self.assertTrue(history.record(2, "up"))
        with self.assertRaises(ValueError):
            history.record(3, "sideways")


if __name__ == "__main__":
    unittest.main(verbosity=2)
