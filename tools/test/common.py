"""Shared helpers for the evaluation and attack scripts.

Python 3 standard library only (tools/README.md). Credentials come from
GATEWAY_USER and GATEWAY_PASSWORD; the base URL comes from the first argument
or GATEWAY_URL, defaulting to the mock server on localhost:8000.
"""

import json
import os
import sys
import urllib.error
import urllib.request

DEFAULT_URL = "http://127.0.0.1:8000"


def base_url():
    if len(sys.argv) > 1 and sys.argv[1].startswith("http"):
        return sys.argv[1].rstrip("/")
    return os.environ.get("GATEWAY_URL", DEFAULT_URL).rstrip("/")


def credentials():
    user = os.environ.get("GATEWAY_USER")
    password = os.environ.get("GATEWAY_PASSWORD")
    if not user or not password:
        sys.exit("set GATEWAY_USER and GATEWAY_PASSWORD before running")
    return user, password


def _json(payload):
    if not payload:
        return {}
    try:
        return json.loads(payload)
    except Exception:
        return {"_raw": payload[:200].decode("utf-8", "ignore")}


class Gateway:
    """Minimal client for the REST API contract in docs/api-contract.md."""

    def __init__(self, url=None):
        self.url = (url or base_url()).rstrip("/")

    def request(self, method, path, body=None, token=None, raw=None):
        data = raw if raw is not None else (
            json.dumps(body).encode() if body is not None else None)
        req = urllib.request.Request(self.url + path, data=data, method=method)
        req.add_header("Content-Type", "application/json")
        if token:
            req.add_header("Authorization", "Bearer " + token)
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                return resp.status, _json(resp.read())
        except urllib.error.HTTPError as exc:
            return exc.code, _json(exc.read())
        except Exception as exc:
            # A refused or dropped connection is a result, not a crash: the
            # contract requires a JSON error body, so silence is a defect.
            return 0, {"error": "transport", "detail": "%s: %s" % (type(exc).__name__, exc)}

    def login(self):
        user, password = credentials()
        code, body = self.request("POST", "/api/login",
                                  {"username": user, "password": password})
        if code != 200:
            sys.exit("login failed: %s %s" % (code, body))
        return body["token"]

    def login_with(self, password):
        user, _ = credentials()
        return self.request("POST", "/api/login",
                            {"username": user, "password": password})

    def reset(self):
        """Mock only: clears sessions, counters and injected failures."""
        return self.request("POST", "/__mock/reset")

    def inject(self, **kwargs):
        """Mock only: simulate link loss, target loss or a forced lockout."""
        return self.request("POST", "/__mock/state", kwargs)

    def output(self, token):
        code, body = self.request("GET", "/api/status", token=token)
        return body.get("output") if code == 200 else None


class Report:
    """Prints PASS/FAIL per check and exits non-zero if any check fails."""

    def __init__(self, title):
        self.title = title
        self.failures = 0
        self.checks = 0
        print(title)

    def check(self, name, ok, detail=""):
        self.checks += 1
        if not ok:
            self.failures += 1
        line = "  %s  %s" % ("PASS" if ok else "FAIL", name)
        if detail:
            line += "  [%s]" % detail
        print(line)

    def finish(self):
        print("  %d/%d checks passed" % (self.checks - self.failures, self.checks))
        sys.exit(0 if self.failures == 0 else 1)


def main_guard(fn):
    if __name__ == "__main__":
        fn()
