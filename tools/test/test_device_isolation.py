#!/usr/bin/env python3
"""Exercise the real PC-only HTTP and mutually authenticated device listeners."""
import copy
import http.client
import json
from pathlib import Path
import socket
import ssl
import sys
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.provision_device_tls import provision
from web.device_server import (DeviceGateway, DeviceHandler, DeviceHTTPServer, DeviceTLSServer,
                               FIXTURES, device_tls_context)

REPORT = {"device_id": "iot-c6", "boot_id": "0000000000000001", "output": "off",
          "ack_id": None, "ack_ok": True, "uptime_s": 1}


class QuietTLSServer(DeviceTLSServer):
    def handle_error(self, request, client_address):
        # Unauthenticated TLS handshakes intentionally fail before HTTP dispatch.
        pass


class IsolationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="device-identities-")
        cls.identity = provision(Path(cls.temporary.name) / "good", "127.0.0.1")
        cls.rogue = provision(Path(cls.temporary.name) / "rogue", "127.0.0.1")

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def setUp(self):
        self.gateway = DeviceGateway(copy.deepcopy(FIXTURES), "test-only-password")
        handler = type("IsolatedHandler", (DeviceHandler,), {"gateway": self.gateway, "quiet": True})
        context, fingerprint = device_tls_context(self.identity)
        self.ui = DeviceHTTPServer(("127.0.0.1", 0), handler, local_ui_only=True)
        self.device = QuietTLSServer(("127.0.0.1", 0), handler, tls_context=context,
                                     device_fingerprint=fingerprint)
        self.threads = [threading.Thread(target=s.serve_forever,
                                        kwargs={"poll_interval": .01}, daemon=True)
                        for s in (self.ui, self.device)]
        for thread in self.threads:
            thread.start()

    def tearDown(self):
        for server in (self.ui, self.device):
            server.shutdown()
            server.server_close()
        for thread in self.threads:
            thread.join(3)

    def client_context(self, identity=None):
        context = ssl.create_default_context(cafile=self.identity / "ca.pem")
        if identity:
            context.load_cert_chain(identity / "device.pem", identity / "device.key")
        return context

    def exchange(self, method, path, body=None, *, secure=False, context=None, source=None):
        kwargs = {"timeout": 3}
        if source:
            kwargs["source_address"] = (source, 0)
        if secure:
            connection = http.client.HTTPSConnection(
                "127.0.0.1", self.device.server_port,
                context=context or self.client_context(self.identity), **kwargs)
        else:
            connection = http.client.HTTPConnection("127.0.0.1", self.ui.server_port, **kwargs)
        try:
            connection.request(method, path, None if body is None else json.dumps(body),
                               {"Content-Type": "application/json"})
            response = connection.getresponse()
            raw = response.read()
            return response.status, json.loads(raw) if raw else None, dict(response.getheaders())
        finally:
            connection.close()

    def test_pc_login_still_works(self):
        status, body, _ = self.exchange("POST", "/api/login",
                                      {"username": "operator", "password": "test-only-password"})
        self.assertEqual(status, 200)
        self.assertIn("token", body)

    def test_unauthenticated_actor_mode_cannot_bind_lan(self):
        with self.assertRaises(ValueError):
            DeviceHTTPServer(("192.0.2.1", 0), DeviceHandler, local_ui_only=False)

    def test_other_source_cannot_use_ui_or_login(self):
        try:
            probe = socket.socket()
            probe.bind(("127.0.0.2", 0))
            probe.close()
        except OSError:
            self.skipTest("Host does not expose a second loopback source address")
        for method, path, body in (("GET", "/api/status", None),
                                   ("POST", "/api/login", {"username": "operator",
                                                           "password": "test-only-password"})):
            status, body, _ = self.exchange(method, path, body, source="127.0.0.2")
            self.assertEqual((status, body), (403, {"error": "pc_only"}))

    def test_plain_device_poll_cannot_change_status(self):
        status, body, _ = self.exchange("POST", "/device/poll", REPORT)
        self.assertEqual((status, body), (403, {"error": "device_tls_required"}))
        self.assertIsNone(self.gateway.last_seen)

    def test_device_certificate_required(self):
        with self.assertRaises((ssl.SSLError, ConnectionError, http.client.RemoteDisconnected)):
            self.exchange("POST", "/device/poll", REPORT, secure=True,
                          context=self.client_context())
        self.assertIsNone(self.gateway.last_seen)

    def test_untrusted_device_certificate_rejected(self):
        with self.assertRaises((ssl.SSLError, ConnectionError, http.client.RemoteDisconnected)):
            self.exchange("POST", "/device/poll", REPORT, secure=True,
                          context=self.client_context(self.rogue))
        self.assertIsNone(self.gateway.last_seen)

    def test_valid_certificate_must_match_provisioned_target(self):
        self.device.device_fingerprint = bytes(32)
        status, body, _ = self.exchange("POST", "/device/poll", REPORT, secure=True)
        self.assertEqual((status, body), (403, {"error": "device_identity_required"}))
        self.assertIsNone(self.gateway.last_seen)

    def test_authenticated_report_and_time(self):
        status, body, headers = self.exchange("POST", "/device/poll", REPORT, secure=True)
        self.assertEqual((status, body), (200, {"command": None}))
        self.assertTrue(self.gateway.target_reachable)
        self.assertTrue(headers["X-Device-Time"].isdigit())

    def test_tls_listener_does_not_expose_browser_api(self):
        status, _, _ = self.exchange("POST", "/api/login", {}, secure=True)
        self.assertEqual(status, 404)

    def test_idle_handshake_does_not_block_authenticated_device(self):
        idle = [socket.create_connection(("127.0.0.1", self.device.server_port), timeout=2)
                for _ in range(4)]
        try:
            self.assertEqual(self.exchange("POST", "/device/poll", REPORT, secure=True)[0], 200)
        finally:
            for connection in idle:
                connection.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
