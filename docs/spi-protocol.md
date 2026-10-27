# SPI internet link

**Contract 0.2.0; wire version 2.** Replaces the unimplemented command protocol.
A is controller; B is the Wi-Fi uplink/peripheral.

## Physical interface

Both ends use SPI2: SCLK 12, MOSI 11, MISO 13, CS 10, HS 4, common ground.
Mode 0, MSB first, 4 MHz initially; 10 MHz maximum after hardware validation.
Retain the PCB termination and CS pull-up/HS pull-down. HS now means **DMA
transaction ready**, not payload available. B raises HS in post_setup_cb and
lowers it in post_trans_cb, with only one queued transaction. A checks HS
before every transfer, polls every 10 ms idle, and drains traffic immediately.

## Framing

Every CS assertion clocks exactly 1600 bytes in both directions. Integers
are little endian; IPv4 addresses in status are network-order octets. Unused
bytes are zero. No SPI retransmissions (TCP handles loss; UDP may lose data).
The peripheral allocates/configures 1604 bytes internally as an overrun guard:
its reported transfer length saturates at its configured capacity. Only exactly
12800 received bits are accepted; the guard is never part of a valid frame.

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | ASCII IGW2 |
| 4 | 1 | Wire version 2 |
| 5 | 1 | HELLO=1, STATUS=2, DATA=3, IDLE=4 |
| 6 | 2 | Flags: MORE=1; others zero |
| 8 | 2 | Payload length |
| 10 | 2 | Reserved zero |
| 12 | 4 | Sequence, starts at 1, strictly increasing within session |
| 16 | 8 | Random nonzero sender session |
| 24 | 8 | Observed peer session; zero only in initial HELLO |
| 32 | 4 | CRC-32/ISO-HDLC over all 1600 bytes with this field zero |
| 36 | 4 | Reserved zero |
| 40 | 0–1514 | Payload, then zero padding |

CRC reflected polynomial 0xedb88320, init/xorout 0xffffffff; `123456789`
checks to 0xcbf43926. HELLO/IDLE payloads are empty. DATA is Ethernet without
FCS, IPv4 or ARP only; no VLAN. IP MTU 1500. STATUS is exactly 56 bytes:

| Payload offset | Bytes | Field |
|---|---|---|
| 0 | 4 | Flags: associated=1, IPv4 acquired=2, forwarding=4, overlap=8, configuration error=16 |
| 4, 8, 12, 16, 20 | 4 each | Upstream IP, mask, gateway, DNS1, DNS2 |
| 24, 28, 32 | 4 each | SPI local IP, peer IP, mask |
| 36, 40 | 4 each | Disconnect reason, current retry delay milliseconds |
| 44, 48, 52 | 4 each | Valid frames received, rejected frames, dropped packets |

## Readiness and ownership

Start with HELLO; exchange and echo session IDs before accepting DATA. Accept
a new peer only via HELLO. Reject duplicate sequences and stale pairing IDs.
An established peer changing closes the data gate and rotates the local
session, preventing old frames from reopening the link. Rotate before sequence
wrap. Two seconds without valid peer traffic invalidates the pairing.
Upstream loss/address change also rotates B's session. Queued packets and DMA
completions are tagged with their original pairing and cannot cross resets.
CRC detects errors, not attacks: physical access to the link remains trusted.

B sends STATUS every 250 ms while paired, including offline. Forwarding means
local route/NAT ready, not verified internet reachability. A enables its data
netif only for matching STATUS with forwarding enabled and matching addresses.
Each endpoint has eight TX packets and eight RX buffers. lwIP owns each RX
buffer until its release callback. Exhaustion drops without blocking.
Teardown closes data gates, disables/clears NAT while netif is up, then
disconnects it. Control traffic continues.

## Addressing

Static SPI defaults: B 192.168.77.1, A 192.168.77.2, /30. Configure both builds
identically. B NATs A onto upstream Wi-Fi; production A NATs its separate
192.168.78.0/24 AP subnet onto its SPI address. No DHCP over SPI. STATUS supplies
upstream DNS, which A advertises through its client DHCP service.
IPv6, VLAN and port forwarding are excluded.

## Changelog

- 0.2.0: IPv4 Ethernet framing, sessions, status, timing, changed HS semantics.
- 0.1.0: PCB pins defined; original command framing remained unspecified.
