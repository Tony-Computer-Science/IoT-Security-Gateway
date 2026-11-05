# Two-wire UART test link

**UART envelope contract 0.2.0; inner gateway wire version 2.**
This optional test transport replaces SPI signals between A and B, keeping
the existing Ethernet frame codec, sessions, status and IPv4 routing policy.
It does not change the SPI 0.2.0 contract or validate SPI electrical behavior.

## Wiring and configuration

With common ground already connected, use two 3.3 V logic signal jumpers:

| Station A | Bridge B |
|---|---|
| GPIO6 TX | GPIO7 RX |
| GPIO7 RX | GPIO6 TX |
| GND | GND |

Keep the individual USB cables for power, flashing and logs. Disconnect old
SPI signal jumpers. GPIO6/7 are available on Espressif C6 DevKitC-1/DevKitM-1;
check the header labels on other boards. UART1 is separate from UART0/USB
console traffic. There are no clock, CS, HS or RTS/CTS wires.

In each project's `idf.py menuconfig`, enable **Component config → Gateway
board link → Use two-wire UART instead of SPI**. Keep **Build Board A endpoint**
enabled on station and disabled on bridge. Match the baud rate; default
921600, 8 data bits, no parity, 1 stop bit. Pins and baud rate are configurable.
Rebuild and flash **both** boards. Tracked defaults continue to select SPI;
local sdkconfig stores the UART selection. Do not repeat `set-target` for a
transport change because it resets other configuration.

Wi-Fi credentials remain required: configure the station AP passphrase and
bridge upstream SSID/passphrase in their own menuconfig sections. An empty
password builds but leaves the corresponding Wi-Fi path unavailable.

## Stream format and behavior

The gateway codec creates a 1600-byte frame. UART omits trailing zero padding,
retaining at least the full 40-byte header, then COBS encodes the compact
record and terminates it with zero. The receiver restores omitted zeros to
1600 bytes before checking the original CRC and message shape.
Transmitters also send a leading zero to recover from a peer's partial frame
after restart. The encoded frame including its trailing delimiter is at most
1608 bytes. Empty delimiters are ignored. Invalid, truncated or oversized
records with invalid COBS or inner CRC are rejected; oversized input is
discarded until the next delimiter. Decoded size must be 40–1600 bytes before
zero restoration; the normal inner CRC, semantic and session checks still
apply. COBS alone does not make a frame acceptable.

Both endpoints independently send HELLO/IDLE/DATA and B sends STATUS. No
request/response or extra handshake wire is used. RX uses an 8192-byte driver
ring while TX blocks, and each bounded drain prevents input from starving
outbound control traffic. Frame parsing, buffers and session rotation remain
owned by one transport task. A frame retains the local session/generation
snapshot captured when assembly starts. A two-second accepted-frame timeout
closes forwarding as with SPI. Unplugging a wire may not be reported until
that timeout. Lost or corrupt records are dropped, not retransmitted by the
link; network protocols retain their normal retry behavior.

Compact control frames and a default 921600 baud avoid the original 115200
baud padded-frame bottleneck. Idle control sends are limited to one every
50 ms; queued data bypasses that idle limit. Throughput is still lower than
SPI. This is for connectivity and recovery checks, not SPI throughput tests.
RX overrun/corruption can cause drops and session recovery under excess load.
The /30 addresses remain A=192.168.77.2 and B=192.168.77.1 by default; routing
and NAT logs may retain legacy SPI terminology.

## Test procedure

1. Verify both startup logs say `UART ready: TX=6 RX=7 baud=921600`.
2. Confirm B associates with upstream Wi-Fi and receives an IPv4 lease.
3. Join A's configured AP, confirm DHCP, then ping A at 192.168.78.1.
4. Try DNS and a small HTTP/HTTPS request through A. IP readiness alone does
   not prove upstream internet access.
5. Disconnect either signal jumper, verify forwarding closes, then reconnect
   and check that a fresh session restores new connections. Repeat by resetting
   either board. Old network connections may need to reconnect.

Run `python3 tools/test/run_host_tests.py` for portable framing, overflow,
resynchronization, inner CRC, session and policy tests. Build both UART images
and the default SPI diagnostic image after changing shared code. Builds and
host tests do not validate physical UART, Wi-Fi, DHCP or NAT behavior.

## Observed hardware checks, 2026-11-05

Two ESP32-C6-WROOM-1 boards (chip revision 0.0) were flashed and tested using
GPIO6 TX and GPIO7 RX, crossed between boards, with common ground. Initial
startup/restart checks used UART1 at 115200 baud. The final client test used
921600 baud and compact envelope 0.2.0. Flash writes verified successfully.

- Station blue and bridge red RGB identity colors were visually confirmed.
- Bridge associated with the configured private upstream network and acquired
  a DHCP IPv4 address.
- UART session/status exchange completed and both routers enabled forwarding.
- Restarting bridge closed station forwarding and cleared client NAT, then
  restored forwarding after bridge reconnected.
- Restarting station closed bridge forwarding and cleared its NAT, then both
  endpoints restored forwarding with a fresh session.
- Hardware testing exposed IDF 6.1 rejecting a zero backup DNS address; the
  station now repeats the learned primary in an absent backup slot. The
  observed DNS-update error was eliminated and station forwarding opened.

- Bridge's independent DNS and HTTP HEAD check passed with HTTP 200.
- Station's independent DNS and HTTP HEAD check through UART/bridge passed.
- With the original slow, padded UART transport, phone traffic filled the
  eight-packet queues and caused sustained drops. Compact frames, 921600 baud
  and idle throttling resolved the observed phone connectivity failure.
  Heavy traffic can still fill these bounded queues and cause packet drops.
- The phone received a DHCP lease, loaded example.com with mobile data off,
  and stopped reporting no internet; this was confirmed by the user on the
  final fast UART build.

A sustained traffic soak and throughput characterization remain untested.
Wi-Fi passwords and generated firmware remain private and are not part of
this record.

## Changelog

- 0.2.0: Omit trailing zero padding and restore it before CRC validation; default
  to 921600 baud and throttle idle control traffic. Update both endpoints.
- 0.1.0: Optional two-wire UART envelope for existing version-2 gateway frames.

For troubleshooting, **Gateway board link → Log packet counters and run a
one-time HTTP connectivity check** enables counters without packet contents
and a single DNS/HTTP HEAD request to example.com on each board after link
readiness. It is disabled in tracked defaults. The station probe checks UART
plus bridge routing independently of a phone's AP/NAT path.
