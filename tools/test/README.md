# Gateway tests

- `run_host_tests.py`: compiles/runs the shared C protocol, sessions and packet
  policy with address/undefined-behavior sanitizers. Requires Python 3 and a C
  compiler; standard library only. Run from any directory.
- `test_gateway_protocol.c`: malformed-wire, CRC, session/reset, addressing and
  pre-NAT firewall regression cases.
- `test_gateway_access.c`: production A's private subnet configuration, B status
  validation, DNS selection, offline DHCP/ping, source spoofing and NAT bypass.
- `echo_server.py --bind <lab-PC-IP> --port 9000`: TCP/UDP echo endpoint for
  the separate ESP32-S3 SPI host; at most eight TCP clients, bounded buffers.

Run network tests only against your own controlled lab. No credentials are
stored in these tools. See [hardware acceptance](../../docs/testing.md) and
[test-host setup](../../firmware/spi-test-host/README.md). The original four
command-gateway evaluation categories have not been implemented; these router
tests do not assert compliance with the original R1–R7 specification.
