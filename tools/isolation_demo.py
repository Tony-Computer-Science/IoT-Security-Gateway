#!/usr/bin/env python3
"""Bounded Linux lab probes for the PC admission gate and gateway boundary.

This tool does not change Wi-Fi, addresses, routes, firmware or firewall rules.
Keep the real device service running. See docs/isolation-demo.md.
"""
import argparse
import http.client
import ipaddress
import json
import os
from pathlib import Path
import re
import socket
import ssl
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def ipv4(value):
    address = ipaddress.IPv4Address(value)
    if not address.is_private or address.is_unspecified or address.is_multicast:
        raise argparse.ArgumentTypeError("use a specific private lab IPv4 address")
    return str(address)


def command(args):
    return subprocess.run(args, capture_output=True, text=True, timeout=5)


def ping(address, source, timeout):
    done = command(["ping", "-n", "-c", "1", "-W", str(max(1, int(timeout))),
                    "-I", source, address])
    return done.returncode == 0 and re.search(r"ttl[= ]\d+", done.stdout, re.I) is not None


def route(address, source):
    done = command(["ip", "-j", "-4", "route", "get", address, "from", source])
    if done.returncode:
        raise RuntimeError("No route to %s from %s" % (address, source))
    return json.loads(done.stdout)[0]


def tcp(address, port, source, timeout):
    try:
        with socket.create_connection((address, port), timeout, (source, 0)):
            return "connected"
    except ConnectionRefusedError:
        return "refused"
    except (socket.timeout, TimeoutError):
        return "timeout"
    except OSError as error:
        return "error:%s" % error.errno


def request(address, source, method, path, body=None, token=None, timeout=3):
    connection = http.client.HTTPConnection(address, 8000, timeout=timeout,
                                           source_address=(source, 0))
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    try:
        connection.request(method, path, None if body is None else json.dumps(body), headers)
        response = connection.getresponse()
        raw = response.read(8192)
        return response.status, json.loads(raw) if raw else None
    finally:
        connection.close()


def no_certificate(address, source, ca_file, timeout):
    context = ssl.create_default_context(cafile=ca_file)
    try:
        with socket.create_connection((address, 8443), timeout, (source, 0)) as connection:
            with context.wrap_socket(connection, server_hostname=address) as secure:
                # TLS 1.3 can deliver the missing-client-certificate alert on read.
                secure.sendall(b"POST /device/poll HTTP/1.1\r\nHost: device\r\n"
                               b"Content-Length: 2\r\nContent-Type: application/json\r\n"
                               b"Connection: close\r\n\r\n{}")
                return "HTTP response: " + secure.recv(128).split(b"\r\n")[0].decode("ascii", "replace")
    except ssl.SSLCertVerificationError:
        return "inconclusive: server certificate verification failed"
    except ssl.SSLError as error:
        reason = error.reason or "unknown TLS error"
        if reason in ("TLSV13_ALERT_CERTIFICATE_REQUIRED", "SSLV3_ALERT_HANDSHAKE_FAILURE"):
            return "client certificate rejected: " + reason
        return "inconclusive: " + reason
    except OSError as error:
        return "inconclusive: " + type(error).__name__


def find_target(mac, interface):
    done = command(["ip", "-j", "neigh", "show", "dev", interface])
    matches = [entry["dst"] for entry in json.loads(done.stdout)
               if entry.get("lladdr", "").lower() == mac.lower()]
    if len(matches) != 1:
        raise RuntimeError("Known target MAC not found uniquely in neighbor cache; supply --target")
    return ipv4(matches[0])


def check(results, name, ok, detail):
    results.append({"check": name, "passed": bool(ok), "detail": detail})
    print("[%s] %s: %s" % ("PASS" if ok else "FAIL/INCONCLUSIVE", name, detail), flush=True)


def denied_command(code, body):
    return code == 403 and isinstance(body, dict) and body.get("error") == "pc_only"


def outside_command(address, source, timeout):
    try:
        code, _ = request(address, source, "POST", "/api/command",
                          {"action": "led_off"}, timeout=timeout)
        return "HTTP %d" % code
    except ConnectionRefusedError:
        return "refused"
    except (socket.timeout, TimeoutError):
        return "timeout"
    except (OSError, ValueError, http.client.HTTPException) as error:
        return "inconclusive: " + type(error).__name__


def inside(args, results):
    print("INSIDE: same-laptop source-IP simulation, not a second physical Wi-Fi client.", flush=True)
    print("Authorized observer: %s; unauthorized source: %s; target: %s" %
          (args.authorized, args.source, args.target), flush=True)
    if args.source == args.authorized:
        raise RuntimeError("Unauthorized source must differ from the authorized PC")
    password = os.environ.get("GATEWAY_PASSWORD")
    if not password:
        raise RuntimeError("Set GATEWAY_PASSWORD privately for the authorized before/after observer")
    code, login = request(args.authorized, args.authorized, "POST", "/api/login",
                          {"username": "operator", "password": password}, timeout=args.timeout)
    if code != 200 or not isinstance(login, dict) or not isinstance(login.get("token"), str):
        raise RuntimeError("Authorized observer login failed; no isolation result can be claimed")
    token = login["token"]
    try:
        code, before = request(args.authorized, args.authorized, "GET", "/api/status", token=token)
        if (code != 200 or not isinstance(before, dict) or not before.get("target_reachable")
                or before.get("output") not in ("on", "off")):
            raise RuntimeError("Target is not reporting fresh state; no isolation result can be claimed")
        print("Live authenticated baseline: target available, LED %s." % before["output"].upper(), flush=True)
        peer_route = route(args.target, args.source)
        if peer_route.get("dev") != args.interface or peer_route.get("gateway"):
            raise RuntimeError("Inside target probe must be directly on the protected Wi-Fi subnet")
        alive = ping(args.target, args.source, args.timeout)
        check(results, "Target discovery/liveness", alive,
              "%s %s a ping from %s (local ping is deliberately permitted)" %
              (args.target, "answered" if alive else "did not answer", args.source))
        allowed_tcp = tcp(args.target, 80, args.authorized, args.timeout)
        peer_tcp = tcp(args.target, 80, args.source, args.timeout)
        check(results, "Target IPv4 application filter",
              alive and allowed_tcp in ("connected", "refused") and peer_tcp == "timeout",
              "same target/port: authorized=%s, unauthorized=%s; a closed-port reply is the control" %
              (allowed_tcp, peer_tcp))
        action = "led_off" if before["output"] == "on" else "led_on"
        code, body = request(args.authorized, args.source, "POST", "/api/command",
                             {"action": action}, token=token, timeout=args.timeout)
        check(results, "Unauthorized LED command", denied_command(code, body),
              "%s from %s, even with an observer session: HTTP %s %s" %
              (action, args.source, code, body.get("error") if isinstance(body, dict) else "invalid body"))
        tls = no_certificate(args.authorized, args.source, args.ca_file, args.timeout)
        check(results, "Device channel requires client identity",
              tls.startswith("client certificate rejected:"), tls)
        code, after = request(args.authorized, args.authorized, "GET", "/api/status", token=token)
        if not isinstance(after, dict):
            raise RuntimeError("Invalid observer status after the attempt")
        check(results, "Device remains available and LED unchanged",
              code == 200 and after.get("target_reachable") and after.get("output") == before["output"],
              "before=%s, after=%s; also observe the physical RGB LED" %
              (before["output"], after.get("output")))
    finally:
        request(args.authorized, args.authorized, "POST", "/api/logout", {}, token=token)


def outside(args, results):
    print("OUTSIDE: upstream Wi-Fi client; protected probes must explicitly route via red.", flush=True)
    print("Source: %s; bridge upstream: %s; protected target: %s" %
          (args.source, args.bridge, args.target), flush=True)
    protected = ipaddress.IPv4Network(args.authorized + "/24", strict=False)
    if ipaddress.IPv4Address(args.source) in protected:
        raise RuntimeError("Outside source is still on the protected AP subnet")
    bridge_route = route(args.bridge, args.source)
    if bridge_route.get("dev") != args.interface or bridge_route.get("gateway"):
        raise RuntimeError("Bridge must be directly reachable on the same upstream LAN")
    bridge_alive = ping(args.bridge, args.source, args.timeout)
    check(results, "Bridge positive control", bridge_alive,
          "upstream bridge %s %s ping" % (args.bridge, "answered" if bridge_alive else "did not answer"))
    if not bridge_alive:
        raise RuntimeError("Cannot establish a live path to red; no firewall result can be claimed")
    for destination in (args.gateway, args.target, args.link_host):
        selected = route(destination, args.source)
        if selected.get("gateway") != args.bridge or selected.get("dev") != args.interface:
            raise RuntimeError("%s is not explicitly routed through the bridge" % destination)
        print("Route: %s via %s dev %s" % (destination, args.bridge, args.interface), flush=True)
    syn = outside_command(args.bridge, args.source, args.timeout)
    check(results, "Outside initial TCP connection", syn == "timeout",
          "LED OFF attempt at upstream port 8000: %s; bridge answers ping, command delivery needs a TCP connection" % syn)
    gateway_ping = ping(args.gateway, args.source, args.timeout)
    target_ping = ping(args.target, args.source, args.timeout)
    link_ping = ping(args.link_host, args.source, args.timeout)
    check(results, "Direct routes into protected networks",
          not gateway_ping and not target_ping and not link_ping,
          "replies from AP gateway=%s, target=%s, private A/B link=%s" %
          (gateway_ping, target_ping, link_ping))
    target_tcp = tcp(args.target, 80, args.source, args.timeout)
    check(results, "Outside target application access", target_tcp == "timeout",
          "%s:80 via red: %s; the target has no LED command listener" % (args.target, target_tcp))
    print("These are boundary observations with a live next-hop control, not per-rule drop counters.", flush=True)
    print("The PC is outside during this check; loss of its device heartbeat is expected and is not a firewall result.", flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("inside", "outside"))
    parser.add_argument("--source", type=ipv4, required=True)
    parser.add_argument("--interface", default="wlan0")
    parser.add_argument("--authorized", type=ipv4, default="192.168.78.10")
    parser.add_argument("--gateway", type=ipv4, default="192.168.78.1")
    parser.add_argument("--target", type=ipv4)
    parser.add_argument("--target-mac", help="locate the known target in the local neighbor cache")
    parser.add_argument("--bridge", type=ipv4, help="red board's current upstream IPv4 address")
    parser.add_argument("--link-host", type=ipv4, default="192.168.77.2")
    parser.add_argument("--ca-file", type=Path, default=ROOT / "local-backups/device-identity/ca.pem")
    parser.add_argument("--timeout", type=float, default=2)
    parser.add_argument("--json", type=Path, help="private result file; tokens/passwords are never included")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout <= 5:
        parser.error("--timeout must be between 1 and 5 seconds")
    if not args.target:
        if args.mode != "inside" or not args.target_mac:
            parser.error("supply --target, or --target-mac for inside neighbor discovery")
    if args.mode == "outside" and not args.bridge:
        parser.error("outside mode requires --bridge")
    results = []
    error = None
    try:
        if not args.target:
            args.target = find_target(args.target_mac, args.interface)
        (inside if args.mode == "inside" else outside)(args, results)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, http.client.HTTPException) as failure:
        error = str(failure)
        print("[INCONCLUSIVE] " + error, flush=True)
    passed = error is None and bool(results) and all(item["passed"] for item in results)
    report = {"mode": args.mode, "source": args.source, "target": args.target,
              "passed": passed, "checks": results, "error": error}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        descriptor = os.open(args.json, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        os.fchmod(descriptor, 0o600)
        with os.fdopen(descriptor, "w") as output:
            json.dump(report, output, indent=2)
            output.write("\n")
    print("\n%s" % ("All measured checks passed." if passed else "Some checks failed or are inconclusive; do not present them as a pass."), flush=True)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
