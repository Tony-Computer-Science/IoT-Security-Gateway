#!/usr/bin/env python3
"""Development mock of the separate web service, serving web/src and its API.

Implements docs/api-contract.md v0.3.1 with real behavior (token issuance and
expiry, failed-login lockout, command rate limit, bounded event log, simulated
LED) and adds /__mock/* endpoints for failure injection so R6 and R7 can be
exercised without hardware. See web/mock/README.md.

Python 3 standard library only. Serves the UI and the API from one origin, so
there is no CORS.

The password is never stored in the repository (R2): it is read from the
MOCK_PASSWORD environment variable or generated randomly at startup.
"""

import argparse
import hmac
import json
import math
import os
import secrets
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

API_VERSION = "0.3.2"

HERE = Path(__file__).resolve().parent
SRC_DIR = (HERE.parent / "src").resolve()
FIXTURES = json.loads((HERE / "fixtures.json").read_text(encoding="utf-8"))

ACTIONS = {"led_on": "on", "led_off": "off"}
OUTCOMES = {"allowed", "denied", "alert"}

CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".svg": "image/svg+xml",
    ".ico": "image/x-icon",
}

# Shared by the mock and real PC web service: same-origin resources only.
# Catches accidental external requests (CDNs, fonts) during development.
CSP = (
    "default-src 'self'; script-src 'self'; style-src 'self'; "
    "img-src 'self' data:; connect-src 'self'; base-uri 'none'; "
    "form-action 'self'; frame-ancestors 'none'"
)

# Injectable state: key -> validator.
INJECTABLE = {
    "link_ok": lambda v: isinstance(v, bool),
    "target_reachable": lambda v: isinstance(v, bool),
    "latency_ms": lambda v: isinstance(v, int) and not isinstance(v, bool) and 0 <= v <= 30000,
    "force_lockout": lambda v: isinstance(v, bool),
}


class ApiError(Exception):
    def __init__(self, status, error, **extra):
        super().__init__(error)
        self.status = status
        self.body = {"error": error, **extra}


class MockGateway:
    """All simulated gateway state. Every public method holds the lock."""

    def __init__(self, fixtures, password):
        self.fx = fixtures
        self.password = password
        self.lock = threading.Lock()
        self.reset()

    # --- lifecycle -------------------------------------------------------

    def reset(self):
        """Simulate a gateway restart: new boot time, no sessions (R2, R7)."""
        init = self.fx["initial_state"]
        self.boot = time.monotonic()
        self.output = init["output"]
        self.link_ok = init["link_ok"]
        self.target_reachable = init["target_reachable"]
        self.latency_ms = init["latency_ms"]
        self.sessions = {}  # token -> monotonic expiry
        self.failed_logins = 0
        self.locked_until = 0.0
        self.command_times = deque()
        self.events = deque(maxlen=self.fx["event_log_capacity"])
        self.seq = 0
        self.last_confirmed_command = None

    def uptime(self):
        return int(time.monotonic() - self.boot)

    def mock_state(self):
        now = time.monotonic()
        return {
            "output": self.output,
            "link_ok": self.link_ok,
            "target_reachable": self.target_reachable,
            "latency_ms": self.latency_ms,
            "locked_out": now < self.locked_until,
            "active_sessions": sum(1 for exp in self.sessions.values() if exp > now),
            "events": len(self.events),
        }

    # --- helpers (caller holds lock) --------------------------------------

    def _log(self, type_, outcome, detail):
        # detail must never contain credentials or tokens (R2).
        self.seq += 1
        self.events.append({
            "seq": self.seq,
            "uptime_s": self.uptime(),
            "type": type_,
            "outcome": outcome,
            "detail": detail,
        })

    def _require_session(self, auth_header):
        now = time.monotonic()
        for tok, exp in list(self.sessions.items()):
            if exp <= now:
                del self.sessions[tok]
        token = None
        if auth_header and auth_header.startswith("Bearer "):
            token = auth_header[len("Bearer "):].strip()
        if not token or token not in self.sessions:
            self._log("auth", "denied", "request with missing or invalid session")
            raise ApiError(401, "invalid_session")
        return token

    # --- endpoints ---------------------------------------------------------

    def login(self, body):
        with self.lock:
            now = time.monotonic()
            if now < self.locked_until:
                self._log("auth", "denied", "login attempt during lockout")
                raise ApiError(429, "locked_out",
                               retry_after=max(1, math.ceil(self.locked_until - now)))
            if (not isinstance(body, dict)
                    or not isinstance(body.get("username"), str)
                    or not isinstance(body.get("password"), str)):
                raise ApiError(400, "invalid_request")

            user_ok = hmac.compare_digest(body["username"].encode(), self.fx["username"].encode())
            pass_ok = hmac.compare_digest(body["password"].encode(), self.password.encode())
            if user_ok and pass_ok:
                self.failed_logins = 0
                token = secrets.token_urlsafe(32)
                self.sessions[token] = now + self.fx["token_ttl_s"]
                self._log("auth", "allowed", "login succeeded")
                return 200, {"token": token, "expires_in": self.fx["token_ttl_s"]}

            limit = self.fx["max_failed_logins"]
            self.failed_logins += 1
            remaining = max(0, limit - self.failed_logins)
            self._log("auth", "denied", "login failed (%d/%d)" % (self.failed_logins, limit))
            if remaining == 0:
                self.failed_logins = 0
                self.locked_until = now + self.fx["lockout_s"]
                self._log("rate_limit", "alert",
                          "login locked out for %ds" % self.fx["lockout_s"])
            raise ApiError(401, "invalid_credentials", attempts_remaining=remaining)

    def logout(self, auth):
        with self.lock:
            token = self._require_session(auth)
            del self.sessions[token]
            self._log("auth", "allowed", "logout")
            return 204, None

    def status(self, auth):
        with self.lock:
            self._require_session(auth)
            return 200, {
                "output": self.output,
                "target_reachable": self.target_reachable,
                "link_ok": self.link_ok,
                "uptime_s": self.uptime(),
                "seq": self.seq,
                "last_confirmed_command": self.last_confirmed_command,
            }

    def command(self, auth, body):
        with self.lock:
            self._require_session(auth)
            now = time.monotonic()

            # R4: sliding-window command rate limit (global in the mock, see Q3).
            rl = self.fx["command_rate_limit"]
            while self.command_times and now - self.command_times[0] >= rl["window_s"]:
                self.command_times.popleft()
            if len(self.command_times) >= rl["max"]:
                retry = max(1, math.ceil(rl["window_s"] - (now - self.command_times[0])))
                self._log("rate_limit", "alert", "command rate limit exceeded")
                raise ApiError(429, "rate_limited", retry_after=retry)
            self.command_times.append(now)

            # R3: allow-list. Do not echo the client's value into the log.
            if not isinstance(body, dict):
                raise ApiError(400, "invalid_request")
            action = body.get("action")
            if not isinstance(action, str) or action not in ACTIONS or set(body) != {"action"}:
                self._log("command", "denied", "invalid action rejected")
                raise ApiError(400, "invalid_action")

            # R7: commands disabled while the link is down. R6: state unchanged.
            if not self.link_ok:
                self._log("command", "denied", "%s rejected: link down" % action)
                raise ApiError(503, "link_down")
            if not self.target_reachable:
                self._log("target", "alert", "%s failed: target unreachable" % action)
                raise ApiError(502, "target_unreachable")

            self.output = ACTIONS[action]
            self.last_confirmed_command = {"action": action, "output": self.output,
                                           "service_uptime_s": self.uptime()}
            self._log("command", "allowed", "%s -> %s" % (action, self.output))
            return 200, {"result": "ok", "output": self.output}

    def events_list(self, auth, query):
        with self.lock:
            self._require_session(auth)
            params = parse_qs(query, keep_blank_values=True)
            if set(params) - {"limit", "outcome"} or any(len(v) != 1 for v in params.values()):
                raise ApiError(400, "invalid_request")

            limit = self.fx["events_default_limit"]
            if "limit" in params:
                raw = params["limit"][0]
                if not raw.isdigit() or not 1 <= int(raw) <= self.fx["event_log_capacity"]:
                    raise ApiError(400, "invalid_request")
                limit = int(raw)
            outcome = params.get("outcome", [None])[0]
            if outcome is not None and outcome not in OUTCOMES:
                raise ApiError(400, "invalid_request")

            result = []
            for ev in reversed(self.events):  # newest first
                if outcome is None or ev["outcome"] == outcome:
                    result.append(ev)
                    if len(result) >= limit:
                        break
            return 200, {"events": result}

    # --- failure injection -------------------------------------------------

    def inject(self, body):
        with self.lock:
            if not isinstance(body, dict) or not body:
                raise ApiError(400, "invalid_request", detail="expected a non-empty JSON object")
            for key, value in body.items():
                if key not in INJECTABLE:
                    raise ApiError(400, "invalid_request",
                                   detail="unknown key %r; allowed: %s" % (key, sorted(INJECTABLE)))
                if not INJECTABLE[key](value):
                    raise ApiError(400, "invalid_request", detail="bad value for %r" % key)
            for key, value in body.items():
                if key == "force_lockout":
                    if value:
                        self.locked_until = time.monotonic() + self.fx["lockout_s"]
                        self._log("rate_limit", "alert", "login lockout forced (mock)")
                    else:
                        self.locked_until = 0.0
                        self.failed_logins = 0
                else:
                    setattr(self, key, value)
            return 200, self.mock_state()

    def do_reset(self):
        with self.lock:
            self.reset()
            return 200, self.mock_state()

    def get_mock_state(self):
        with self.lock:
            return 200, self.mock_state()

    def current_latency(self):
        with self.lock:
            return self.latency_ms


# Route table: path -> {method: handler(gateway, request) -> (status, body)}
ROUTES = {
    "/api/login": {"POST": lambda g, r: g.login(r.json_body())},
    "/api/logout": {"POST": lambda g, r: g.logout(r.auth())},
    "/api/status": {"GET": lambda g, r: g.status(r.auth())},
    "/api/command": {"POST": lambda g, r: g.command(r.auth(), r.json_body())},
    "/api/events": {"GET": lambda g, r: g.events_list(r.auth(), r.query)},
    "/__mock/state": {"GET": lambda g, r: g.get_mock_state(),
                      "POST": lambda g, r: g.inject(r.json_body())},
    "/__mock/reset": {"POST": lambda g, r: g.do_reset()},
}


class Handler(BaseHTTPRequestHandler):
    server_version = "gateway-mock/" + API_VERSION
    sys_version = ""
    gateway = None  # set in main()
    quiet = False

    # --- request helpers ---------------------------------------------------

    def auth(self):
        return self.headers.get("Authorization")

    def json_body(self):
        length = self.headers.get("Content-Length")
        if length is None:
            return None
        try:
            n = int(length)
        except ValueError:
            raise ApiError(400, "invalid_request")
        if n < 0 or self.headers.get("Transfer-Encoding") is not None:
            self.close_connection = True
            raise ApiError(400, "invalid_request")
        if n > self.gateway.fx["max_body_bytes"]:
            self.close_connection = True
            raise ApiError(413, "payload_too_large")
        raw = self.rfile.read(n)
        try:
            return json.loads(raw.decode("utf-8")) if raw else None
        except (UnicodeDecodeError, json.JSONDecodeError):
            raise ApiError(400, "invalid_request")

    def send_json(self, status, body):
        data = b"" if body is None else json.dumps(body).encode("utf-8")
        self.send_response(status)
        if body is not None:
            self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if data:
            self.wfile.write(data)

    # --- dispatch -------------------------------------------------------------

    def dispatch(self, method):
        parts = urlsplit(self.path)
        path, self.query = parts.path, parts.query

        if path.startswith("/api/") or path.startswith("/__mock/"):
            try:
                routes = ROUTES.get(path)
                if routes is None:
                    raise ApiError(404, "not_found")
                handler = routes.get(method)
                if handler is None:
                    raise ApiError(405, "method_not_allowed")
                if path.startswith("/api/"):
                    latency = self.gateway.current_latency()
                    if latency:
                        time.sleep(latency / 1000.0)
                status, body = handler(self.gateway, self)
            except ApiError as e:
                status, body = e.status, e.body
            self.send_json(status, body)
            return

        if method != "GET":
            self.send_error(405)
            return
        self.serve_static(path)

    def serve_static(self, path):
        rel = "index.html" if path in ("", "/") else path.lstrip("/")
        target = (SRC_DIR / rel).resolve()
        try:
            target.relative_to(SRC_DIR)
        except ValueError:
            self.send_error(404)
            return
        if not target.is_file():
            self.send_error(404)
            return
        data = target.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", CONTENT_TYPES.get(target.suffix, "application/octet-stream"))
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Security-Policy", CSP)
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        self.dispatch("GET")

    def do_POST(self):
        self.dispatch("POST")

    def do_PUT(self):
        self.dispatch("PUT")

    def do_PATCH(self):
        self.dispatch("PATCH")

    def do_DELETE(self):
        self.dispatch("DELETE")

    def log_message(self, fmt, *args):
        # Default format logs only the request line: never headers, so the
        # bearer token is never written to the console (R2).
        if not self.quiet:
            sys.stderr.write("  %s\n" % (fmt % args))


def main():
    parser = argparse.ArgumentParser(description="Mock IoT gateway API + static server for web/src.")
    parser.add_argument("--host", default="127.0.0.1", help="bind address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8000, help="port (default: 8000)")
    parser.add_argument("--quiet", action="store_true", help="do not log each request")
    args = parser.parse_args()

    password = os.environ.get("MOCK_PASSWORD")
    generated = not password
    if generated:
        password = secrets.token_urlsafe(9)

    Handler.gateway = MockGateway(FIXTURES, password)
    Handler.quiet = args.quiet
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    base = "http://%s:%d" % (args.host, args.port)

    print("IoT gateway mock server (API contract v%s)" % API_VERSION)
    print("  Open:     %s/" % base)
    print("  Username: %s" % FIXTURES["username"])
    if generated:
        print("  Password: %s   (random per run; set MOCK_PASSWORD to choose one)" % password)
    else:
        print("  Password: (from MOCK_PASSWORD)")
    print("  Inject:   curl -X POST %s/__mock/state -d '{\"link_ok\": false}'" % base)
    print("  Reset:    curl -X POST %s/__mock/reset" % base)
    print("  Docs:     web/mock/README.md        Ctrl+C to stop")
    sys.stdout.flush()

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        httpd.server_close()


if __name__ == "__main__":
    main()
