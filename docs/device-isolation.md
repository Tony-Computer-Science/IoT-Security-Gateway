# PC-only target communication

The PC hosts the login/control/events UI on port 8000. Only connections
originating on that PC (its bound IPv4 address or IPv4 loopback) may use the
UI/API. Other clients receive `403 pc_only`. The mock CLI applies the same
browser admission rule.

The target initiates HTTPS polls to the PC on port 8443. Both endpoints
require certificates from a private CA. The PC also pins the exact target
certificate. The target checks the PC certificate chain and server IP.
Plaintext device polls are rejected; no unauthenticated fallback exists.

## Provision and deploy

One-time setup requires OpenSSL on PATH; runtime uses Python's standard
library and ESP-IDF. Run from the repository root on the UI PC:

```sh
python3 tools/provision_device_tls.py
```

This creates `local-backups/device-identity/`, ignored by Git, with private
directory/file permissions. Keep keys, certificates, raw flash images and
credential-bearing builds private. The CA signing key and PC key stay on the
PC. The target embeds only its client key/certificate and CA certificate.

Configure and rebuild Board C with the station AP credentials, fixed PC IPv4
address and device TLS port 8443. CMake defaults to that identity directory;
use `-D TARGET_IDENTITY_DIR=<private-directory>` when building elsewhere.
A missing identity stops the build. Use dummy identities only for compilation
tests, never on a real target. Then start the PC service normally:

```sh
python3 web/device_server.py --host 192.168.78.10 --port 8000
```

Use `--identity-dir` or `--device-port` only with matching target settings.
Open the ordinary browser URL on the PC and sign in. No browser certificate
installation, JavaScript dependencies or Python packages are added.

For rotation, provision a new private directory, rebuild the target and
restart the service. The tool refuses to overwrite an existing identity.
Certificate date checks are enabled. The target initially estimates time
from firmware build time, then refines it from the authenticated PC time
header. Its first connection has no independently trusted real-time clock.
Chain/name checks stay enabled; rebuild after rotation or long storage.

## Packet boundary and limits

The target's IPv4 input hook permits only the configured PC source address,
plus unfragmented DHCP replies (UDP 67 → 68) and ICMP echo request/reply
(type 8/0, code 0) from unicast addresses on the AP /24. Pings between local
devices are allowed; other non-PC application traffic remains blocked. IPv6 is disabled. ARP and Wi-Fi
management still operate; this is not AP-wide layer-2 client isolation.
Other clients keep internet access. IP/MAC spoofing cannot replace the PC's
private key, but hostile peers can still disrupt traffic or shared airtime.
A compromised authorized PC or physical key extraction is outside this scope.

The UI's reachability indicator is not a continuous isolation test. Keep
cryptographic, packet-policy and hardware observations separate. Run
`bash ci/run.sh` for real TLS rejection tests, service/browser tests, sanitized
parser/input-policy tests and asset validation. On hardware verify authorized
control, rejected non-PC traffic, certificate failures, reconnection and
ordinary client internet access. A source-IP alias probe on the PC is packet
evidence, not a second physical radio client or a complete spoofing/DoS test.
