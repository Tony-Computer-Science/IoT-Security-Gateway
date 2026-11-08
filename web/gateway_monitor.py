"""Live gateway checks run by the PC service, plus short up/down histories.

No firmware support is needed. In the fixed-IP demo the PC sits on Board A's
private Wi-Fi and routes through it. Board A's forwarding policy
(firmware/common/gateway_protocol/gateway_access.c) then gives two
observable facts:

* Board A answers ICMP echo on its AP address whenever its AP is up, even
  while the UART link is down. A ping therefore checks Board A.
* Client traffic to addresses outside the AP and link subnets is forwarded
  only while the A-B link is ready, and only reaches the outside if Board B's
  upstream Wi-Fi and NAT work. A TCP connection to an outside address
  therefore checks the whole path A -> UART -> B -> upstream at once.

The path check cannot tell a UART fault from a Board B upstream fault. It is
also only meaningful while the PC's route to that address goes through
Board A; the README tells the operator to keep the PC on that one network.
"""
from collections import deque
import ipaddress
import re
import socket
import subprocess
import sys
import threading
import time

STATES = ("unknown", "up", "down", "off")
TTL = re.compile(rb"ttl[=:]\s*\d+", re.IGNORECASE)
PING_TIME = re.compile(rb"time[=<]\s*([0-9]+(?:\.[0-9]+)?)\s*ms", re.IGNORECASE)


class History:
    """State transitions within a sliding window, plus the state before it."""

    def __init__(self, window_s, start_s=0.0, state="unknown"):
        self.window_s = window_s
        self.points = deque([(start_s, state)])

    @property
    def state(self):
        return self.points[-1][1]

    def record(self, t_s, state):
        if state not in STATES:
            raise ValueError("unknown state %r" % state)
        if state == self.state:
            return False
        self.points.append((t_s, state))
        return True

    def snapshot(self, now_s):
        # Keep the last transition at or before the window start so the strip
        # knows which state was in effect when the window opens.
        start = now_s - self.window_s
        while len(self.points) > 1 and self.points[1][0] <= start:
            self.points.popleft()
        return [{"t": round(t, 1), "state": s} for t, s in self.points]


class Check:
    """One periodic check with failure debouncing and a history."""

    def __init__(self, name, probe, interval_s, target, window_s, fail_threshold=2,
                 up_detail="", down_detail=""):
        self.name = name
        self.probe = probe
        self.interval_s = interval_s
        self.target = target
        self.fail_threshold = fail_threshold
        self.up_detail = up_detail
        self.down_detail = down_detail
        self.history = History(window_s)
        self.failures = 0
        self.latency_ms = None
        self.checked_at = None
        self.due_at = 0.0

    def apply(self, ok, latency_ms, now_s):
        """Fold one probe result in; return the new state if it changed."""
        self.checked_at = now_s
        if ok:
            self.failures = 0
            self.latency_ms = latency_ms
            new = "up"
        else:
            self.failures += 1
            self.latency_ms = None
            # One lost ping is noise; repeated failures are an outage.
            new = "down" if self.failures >= self.fail_threshold else self.history.state
        return new if self.history.record(now_s, new) else None

    def snapshot(self, now_s):
        return {
            "state": self.history.state,
            "target": self.target,
            "latency_ms": None if self.latency_ms is None else round(self.latency_ms, 1),
            "checked_s_ago": None if self.checked_at is None else round(now_s - self.checked_at, 1),
            "history": self.history.snapshot(now_s),
        }


def ping_command(ip, timeout_s, platform=None):
    platform = platform or sys.platform
    if platform.startswith("win"):
        return ["ping", "-n", "1", "-w", str(int(timeout_s * 1000)), ip]
    if platform == "darwin":
        return ["ping", "-n", "-c", "1", "-W", str(int(timeout_s * 1000)), ip]
    return ["ping", "-n", "-c", "1", "-W", str(max(1, int(timeout_s))), ip]


def parse_ping(returncode, output):
    """A reply needs exit 0 AND a TTL: Windows exits 0 on 'host unreachable'."""
    if returncode != 0 or not TTL.search(output):
        return False, None
    match = PING_TIME.search(output)
    return True, float(match.group(1)) if match else None


def ping_probe(ip, timeout_s=1.0, run=subprocess.run):
    ipaddress.IPv4Address(ip)  # never hand anything else to a subprocess
    try:
        done = run(ping_command(ip, timeout_s), capture_output=True, timeout=timeout_s + 2)
    except (OSError, subprocess.TimeoutExpired):
        return False, None
    return parse_ping(done.returncode, done.stdout)


def tcp_probe(host, port, timeout_s=2.0, connect=socket.create_connection, clock=time.monotonic):
    started = clock()
    try:
        with connect((host, port), timeout=timeout_s):
            return True, (clock() - started) * 1000
    except OSError:
        return False, None


def parse_outside(value):
    """'a.b.c.d:port' with a literal IPv4 address, so DNS is not part of the check."""
    host, sep, port = value.rpartition(":")
    if not sep:
        raise ValueError("expected IPv4:port")
    ipaddress.IPv4Address(host)
    port = int(port)
    if not 1 <= port <= 65535:
        raise ValueError("port out of range")
    return host, port


class GatewayMonitor:
    """Runs Board A and gateway-path checks and folds results into a DeviceGateway."""

    def __init__(self, gateway, board_a_probe=None, path_probe=None, *, gateway_ip=None,
                 outside=None, board_a_interval_s=2.0, path_interval_s=5.0):
        self.gateway = gateway
        self.enabled = board_a_probe is not None and path_probe is not None
        window = gateway.history_window_s
        self.checks = {}
        if self.enabled:
            self.checks["board_a"] = Check(
                "board_a", board_a_probe, board_a_interval_s, gateway_ip, window,
                up_detail="Board A answering ping",
                down_detail="Board A not answering ping")
            self.checks["gateway_path"] = Check(
                "gateway_path", path_probe, path_interval_s, outside, window,
                up_detail="traffic passing through the gateway",
                down_detail="no traffic through the gateway: UART link or Board B upstream down")
        self.stop_event = threading.Event()
        self.thread = None

    def tick(self):
        """Run every check that is due. Probes run outside the gateway lock."""
        now = self.gateway.now_s()
        results = []
        for check in self.checks.values():
            if now >= check.due_at:
                check.due_at = now + check.interval_s
                results.append((check, check.probe()))
        with self.gateway.lock:
            # Also expire a silent device promptly, even with no browser polling.
            self.gateway._refresh()
            for check, (ok, latency_ms) in results:
                changed = check.apply(ok, latency_ms, self.gateway.now_s())
                if changed == "up":
                    self.gateway._log("gateway", "allowed", check.up_detail)
                elif changed == "down":
                    self.gateway._log("gateway", "alert", check.down_detail)

    def diagnosis(self):
        if not self.enabled:
            return "not_monitored"
        a = self.checks["board_a"].history.state
        path = self.checks["gateway_path"].history.state
        if a == "down":
            return "board_a_down"
        if a == "unknown" or path == "unknown":
            return "checking"
        return "link_or_upstream_down" if path == "down" else "forwarding"

    def snapshot(self):
        """Caller holds the gateway lock."""
        now = self.gateway.now_s()
        off = {"state": "off", "target": None, "latency_ms": None, "checked_s_ago": None,
               "history": []}
        return {
            "enabled": self.enabled,
            "now_s": round(now, 1),
            "window_s": self.gateway.history_window_s,
            "diagnosis": self.diagnosis(),
            "board_a": self.checks["board_a"].snapshot(now) if self.enabled else off,
            "gateway_path": self.checks["gateway_path"].snapshot(now) if self.enabled else off,
            "device": {"state": self.gateway.device_history.state,
                       "history": self.gateway.device_history.snapshot(now)},
        }

    def start(self, period_s=0.5):
        def run():
            while not self.stop_event.is_set():
                try:
                    self.tick()
                except Exception as error:  # keep monitoring alive; never crash the service
                    sys.stderr.write("gateway monitor error: %s\n" % type(error).__name__)
                self.stop_event.wait(period_s)
        self.thread = threading.Thread(target=run, name="gateway-monitor", daemon=True)
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        if self.thread:
            self.thread.join(3)
