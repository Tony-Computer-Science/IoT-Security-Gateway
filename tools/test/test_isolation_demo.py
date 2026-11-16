#!/usr/bin/env python3
"""Prevent unavailable devices or missing routes from becoming isolation passes."""
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import isolation_demo as demo


class IsolationDemoTests(unittest.TestCase):
    def run_demo(self, mode, **patches):
        source = "192.168.78.11" if mode == "inside" else "10.20.1.50"
        args = [mode, "--source", source, "--target", "192.168.78.100"]
        if mode == "outside":
            args += ["--bridge", "10.20.1.209"]
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "result.json"
            stream = io.StringIO()
            with patch.multiple(demo, **patches), redirect_stdout(stream), \
                    patch.dict("os.environ", {"GATEWAY_PASSWORD": "private-test-password"}):
                code = demo.main(args + ["--json", str(output)])
            return code, json.loads(output.read_text()), stream.getvalue()

    def outside_routes(self, destination, source):
        return {"dev": "wlan0", **({} if destination == "10.20.1.209"
                                  else {"gateway": "10.20.1.209"})}

    def test_outside_offline_bridge_cannot_count_as_firewall(self):
        tcp = Mock()
        code, report, _ = self.run_demo("outside", route=Mock(side_effect=self.outside_routes),
                                        ping=Mock(return_value=False), tcp=tcp)
        self.assertEqual(code, 1)
        self.assertFalse(report["passed"])
        self.assertIn("no firewall result", report["error"])
        tcp.assert_not_called()

    def test_outside_default_router_is_not_the_gateway_under_test(self):
        tcp = Mock()
        code, report, _ = self.run_demo(
            "outside", route=Mock(side_effect=[{"dev": "wlan0"},
                                               {"dev": "wlan0", "gateway": "10.20.1.1"}]),
            ping=Mock(return_value=True), tcp=tcp)
        self.assertEqual(code, 1)
        self.assertIn("not explicitly routed", report["error"])
        tcp.assert_not_called()

    def test_outside_refused_connection_is_not_a_silently_dropped_syn(self):
        code, report, _ = self.run_demo(
            "outside", route=Mock(side_effect=self.outside_routes),
            ping=Mock(side_effect=[True, False, False, False]),
            outside_command=Mock(return_value="refused"), tcp=Mock(return_value="timeout"))
        self.assertEqual(code, 1)
        self.assertFalse(report["checks"][1]["passed"])

    def test_outside_live_next_hop_and_explicit_routes_allow_boundary_result(self):
        code, report, _ = self.run_demo(
            "outside", route=Mock(side_effect=self.outside_routes),
            ping=Mock(side_effect=[True, False, False, False]),
            outside_command=Mock(return_value="timeout"),
            tcp=Mock(return_value="timeout"))
        self.assertEqual(code, 0)
        self.assertTrue(report["passed"])

    def inside_request(self, address, source, method, path, body=None, token=None, **kwargs):
        if path == "/api/login":
            return 200, {"token": "private-test-session-token"}
        if path == "/api/status":
            return 200, {"target_reachable": True, "output": "on"}
        if path == "/api/command":
            return 403, {"error": "pc_only"}
        return 204, None

    def test_inside_offline_target_cannot_count_as_isolation(self):
        tcp = Mock()
        code, report, _ = self.run_demo(
            "inside", request=Mock(side_effect=[(200, {"token": "private-token"}),
                                                (200, {"target_reachable": False, "output": "on"}),
                                                (204, None)]), tcp=tcp)
        self.assertEqual(code, 1)
        self.assertIn("not reporting fresh state", report["error"])
        tcp.assert_not_called()

    def test_inside_all_silent_tcp_probes_are_inconclusive(self):
        code, report, _ = self.run_demo(
            "inside", request=Mock(side_effect=self.inside_request),
            route=Mock(return_value={"dev": "wlan0"}), ping=Mock(return_value=True),
            tcp=Mock(return_value="timeout"),
            no_certificate=Mock(return_value="client certificate rejected: TLSV13_ALERT_CERTIFICATE_REQUIRED"))
        self.assertEqual(code, 1)
        self.assertFalse(report["checks"][1]["passed"])

    def test_inside_evidence_never_contains_observer_credentials(self):
        code, report, console = self.run_demo(
            "inside", request=Mock(side_effect=self.inside_request),
            route=Mock(return_value={"dev": "wlan0"}), ping=Mock(return_value=True),
            tcp=Mock(side_effect=["refused", "timeout"]),
            no_certificate=Mock(return_value="client certificate rejected: TLSV13_ALERT_CERTIFICATE_REQUIRED"))
        self.assertEqual(code, 0)
        for secret in ("private-test-password", "private-test-session-token"):
            self.assertNotIn(secret, console + json.dumps(report))

    def test_inside_wrong_tls_server_identity_is_inconclusive(self):
        code, report, _ = self.run_demo(
            "inside", request=Mock(side_effect=self.inside_request),
            route=Mock(return_value={"dev": "wlan0"}), ping=Mock(return_value=True),
            tcp=Mock(side_effect=["refused", "timeout"]),
            no_certificate=Mock(return_value="inconclusive: server certificate verification failed"))
        self.assertEqual(code, 1)
        self.assertFalse(report["checks"][3]["passed"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
