# Architecture: IPv4 router milestone

```text
Client devices
    | WPA2 personal Wi-Fi (AES/CCMP)
Board A: private AP, client DHCP, NAT onto its SPI address
    | SPI2: SCLK/MOSI/MISO/CS/HS, no additional network connection
Board B: virtual Ethernet -> IPv4 NAPT -> personal Wi-Fi station
    | existing Wi-Fi router
Upstream LAN / internet
```

Repository names remain station-board=A and bridge-board=B, but their planned
Wi-Fi roles are now A=AP and B=station. Production A provides AP/DHCP/NAT.
The separate spi-test-host image stands in for A with its Wi-Fi radio disabled.
Board C now joins A's AP as a Wi-Fi client and polls the UI PC at
192.168.78.10:8000. The PC serves the existing browser API and confirms LED
commands using device acknowledgements. This application traffic stays on
the AP; it does not pass through the A/B transport or require outside access.
The intentionally unauthenticated device channel is documented in
[device-api.md](device-api.md). Browser login authenticates operators to the
PC service; it does not authenticate C or implement per-client network policy.

For tests with two signal wires plus common ground, both endpoints can select
[UART test mode](uart-link.md) instead of SPI. It carries the same Ethernet
frames, status and sessions with a serial envelope. Routing and subnet rules
below are unchanged; existing SPI names also refer to this virtual interface
when UART is selected. The SPI path remains the production PCB default.

## Addressing and data path

SPI uses static B=192.168.77.1, A=192.168.77.2, mask=255.255.255.252.
Both images expose these settings in menuconfig and require a usable /30 pair.
B rejects upstream subnet overlap. A defaults to 192.168.78.1/24, supplies DHCP
leases .100-.199 and requires a private .1/24 distinct from SPI and upstream.
A NATs its clients onto its SPI address.
This intentionally uses NAT on each board and requires no upstream static route.
Only B obtains an upstream address using DHCP. DNS addresses travel in SPI
STATUS; the test host installs them into its resolver and production A advertises
them to clients through DHCP. No DHCP runs over SPI. A keeps its AP/DHCP active
through ordinary upstream outages. On a DNS address change, it restarts DHCP and
briefly reassociates clients so they obtain current options; before first upstream
readiness DNS offering is disabled, though IDF 6.1 may advertise the AP
gateway as its DHCP DNS fallback. No local DNS proxy or public fallback is
used. A missing backup repeats the learned primary in the SDK DNS slots,
since its public API rejects clearing a slot with 0.0.0.0.

Ethernet frames are copied through bounded eight-packet queues/pools. The
shared codec handles wire framing independently of ESP-IDF. The shared link
component attaches to ESP-NETIF's Ethernet stack. B enables NAPT on the SPI
netif and sets the upstream Wi-Fi netif as its default route. Ordinary
outbound IPv4 TCP, UDP and ICMP are supported. HTTPS remains end-to-end.

## Failure boundaries

- Data is gated on an agreed SPI session and upstream IPv4 readiness without
  subnet overlap. An IP lease is not an internet reachability assertion.
- Timeout (2 seconds), peer restart, uplink loss or address change closes the
  gate, invalidates session state and discards queued traffic. NAT is cleared
  before reopening. Control/status remains available while offline.
- NAT is enabled before opening the data gate. It is disabled while the netif
  is still up, before taking the netif down. lwIP forwarding hooks also check
  readiness so already queued packets cannot bypass a closed gate.
- The ingress hook rejects direct upstream destinations in the SPI subnet;
  adding an upstream static route cannot bypass NAT. Port mapping is disabled.
  Only traffic matching an outbound NAT mapping reaches the downstream host.
- SPI IPv4 source addresses must equal A's configured SPI address. Malformed
  L3/L4 input, IPv6, VLAN and unknown EtherTypes are dropped. Counters are
  available through STATUS; logs omit credentials and packet contents.
- The SDK's NAPT path parses transport headers before IP reassembly, so this
  milestone drops IPv4 fragments before that path. TCP uses MTU 1500/MSS 1460;
  oversized fragmented UDP is unsupported. ICMP echo works; this is not a
  general-purpose ICMP error/ALG implementation. ESP-IDF NAPT creates outbound
  TCP/UDP mappings for source ports >=1024, as used by ordinary client sockets.
- SPI is trusted by physical control. CRC detects corruption, not malicious
  peers. A enforces WPA2/AES admission and basic IPv4 source/subnet policy;
  application authorization and per-device policy remain future work. This
  does not implement Wi-Fi client isolation.

## Original graded requirements

[requirements.md](requirements.md) is preserved as the original specification,
not rewritten to make the new router appear compliant.

| Original requirement | Router milestone consequence |
|---|---|
| R1 controlled target entry | Topology and traffic direction changed; target-command isolation is not implemented here. |
| R2 credential before commands | Uplink/AP credentials are protected from source/log disclosure and A requires WPA2 admission; application command authentication is not part of this router. |
| R3 status/output allow-list, fixed destination | Intentionally incompatible: clients choose ordinary IPv4 destinations. |
| R4 configurable request limits | Packet queues are bounded; the PC service reuses browser login/command limits and allows one pending target command. These are separate from router policy. |
| R5 bounded authorized event log | The PC service exposes an authenticated bounded application log. Router link counters and serial diagnostics are separate; the UI events view is unfinished. |
| R6 normal target output control | C6 LED control and PC command confirmation are implemented and tested locally; third-board hardware acceptance is pending. |
| R7 restart handling | Session invalidation and closed data paths are implemented, but physical acceptance is pending. |

The [SPI 0.2.0 contract](spi-protocol.md) supersedes the draft command protocol.
The [REST API 0.2.0](api-contract.md) is served by the PC, not A or B, and
documents unknown command results when a delivered command lacks confirmation.
PCB signal routing remains valid, but its A-uplink/B-IoT-AP labels reflect the
old architecture and must be revised before fabrication.

A fixed UI host address (.10 in the default AP /24) is excluded from the DHCP
pool. Station supports four simultaneous clients by default; associated
clients can communicate locally with the UI PC. Both pre-NAT filters reject
new TCP SYN packets arriving from outside while allowing normal replies.
See [client addressing and outbound-only routing](client-network.md).
