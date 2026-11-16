# Inside/outside isolation recording

Use `python3 tools/isolation_demo.py --help`. The Linux tool uses Python's
standard library plus `ip` and `ping`. It changes no network settings and
contacts only the supplied lab addresses, with one bounded probe per check.
It never scans the upstream network or guesses passwords.

Keep the real PC service running on `192.168.78.10:8000`, with device mutual
TLS on 8443. The ESP32 has no LED command listener: it polls that service.
Stopping the service or changing its compiled authorized address would make
failed commands ambiguous. The browser window can be closed for the recording.
Keep credentials, certificates and private result files out of the video.

## Inside the protected network

Keep the PC at `.10` on IoT-Gateway. Add a temporary unused source address
outside the DHCP pool, such as `.11`, to that same adapter. On a NetworkManager
Linux computer, an example runtime-only change is:

```sh
nmcli device modify wlan0 +ipv4.addresses 192.168.78.11/24
```

Supply the operator password privately through `GATEWAY_PASSWORD`, never a
command-line argument. The script uses it only for a separately labeled
authorized observer, which verifies a fresh device report before and after
the attempts. Do not copy target/server private keys into an attacker client.

```sh
python3 tools/isolation_demo.py inside --source 192.168.78.11 \
  --target <target-IP> --json local-backups/isolation-inside.json
```

Alternatively supply `--target-mac <known-target-MAC>` instead of `--target`.
This locates the known board in the local neighbor cache; it is not arbitrary
device discovery. If no unique match exists, obtain the address from the target
startup log or a prior authorized connection and use `--target`.

The recording shows:

1. A fresh authenticated report and the current LED state.
2. A ping reply from the unauthorized source: local ping is deliberately allowed.
3. The same target TCP port responds to the authorized source (normally a
   closed-port refusal) but times out from the unauthorized source. Both
   requests timing out is inconclusive, not a filter pass.
4. A request to change the LED to the opposite state receives `403 pc_only`.
   This request deliberately carries the observer's existing session to test
   source admission even when an otherwise valid session is present. No
   session value is printed or written to results.
5. The device TLS listener rejects a connection without a client certificate.
   Server certificate validation remains enabled. Wrong server identity or
   generic transport failure is inconclusive.
6. A fresh device report still shows the original LED state. Observe the real
   RGB LED too.

Remove only the temporary address added for this run afterward:

```sh
nmcli device modify wlan0 -ipv4.addresses 192.168.78.11/24
```

This is a same-computer source-IP simulation, not a second physical Wi-Fi
client, MAC/spoofing test, or AP-wide layer-2 isolation test.

## Outside the protected network

Put the probe computer on **the same upstream LAN as red**, not on IoT-Gateway.
Use the actual upstream address reported by red; do not assume a saved DHCP
lease is current. First verify red answers ping from that upstream client.

Explicitly install host routes through red for three known destinations:
the AP gateway (`192.168.78.1`), the target IP, and Board A's link address
(`192.168.77.2`). For example, replace `<bridge-upstream-IP>` and `<target-IP>`
in these temporary NetworkManager adapter changes:

```sh
nmcli device modify wlan0 +ipv4.routes "192.168.78.1/32 <bridge-upstream-IP>"
nmcli device modify wlan0 +ipv4.routes "<target-IP>/32 <bridge-upstream-IP>"
nmcli device modify wlan0 +ipv4.routes "192.168.77.2/32 <bridge-upstream-IP>"
python3 tools/isolation_demo.py outside --source <upstream-PC-IP> \
  --bridge <bridge-upstream-IP> --target <target-IP> \
  --json local-backups/isolation-outside.json
```

The tool requires red to be directly reachable and checks every protected
route names red as the next hop. It then measures a new incoming TCP
connection, direct private-network ping attempts and target application
access. If red is unreachable or the routes are missing/wrong, it stops with
an inconclusive result. This avoids presenting an unrelated home router's
missing route as the ESP32 firewall.

These are end-to-end gateway/NAT boundary observations, not per-rule drop
counters. In particular, red explicitly filters direct access into the A/B
link subnet and new incoming TCP SYN packets. Other protected destinations
also depend on routing/NAT; the result does not identify a single drop rule.

With only one PC, moving it outside takes the authorized polling service off
the protected network. The target's heartbeat will expire for that reason;
do not present that expiry as a firewall result. The blue AP and red bridge
remain independent live controls. Keep the LED in frame and note its state.

Rejoin the original IoT-Gateway profile afterward. The runtime-only routes
are discarded on reconnection. Verify `.10`, a fresh device report and the
original LED state again. A second physical PC, with the authorized service
left inside, is a stronger simultaneous-client demonstration.

## Evidence

Use `--json local-backups/<name>.json` for a private result file. Console and
JSON results contain no passwords or session tokens. Any failure or
inconclusive check makes the tool exit nonzero; silence alone is not a pass.
Keep software tests, these hardware observations and any remaining physical
acceptance separate, as in [device-isolation.md](device-isolation.md).
