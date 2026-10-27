# Shared firmware components

- `gateway_protocol`: ESP-IDF-independent C codec, session state, status format,
  address checks and pre-NAT IPv4 policy. `gateway_access` contains production
  A's client/bridge readiness and DHCP exceptions. Tested by tools/test/run_host_tests.py.
- `gateway_link`: ESP-IDF 6.1 Ethernet netif and full-duplex SPI driver. Compiles
  as peripheral for B and controller for production A or the separate test host. Eight TX
  packets, eight RX buffers, two DMA buffers; no PSRAM dependency.

`gateway_link_set_netif_up(true)` brings up the netif but does not open the gate.
The supervisor must establish routing/NAT first, then call
`gateway_link_open_gate()`. To stop: close gate, disable NAT while the netif is
up, then `gateway_link_set_netif_up(false)`. Only one supervisor owns netif lifecycle. Reset may be
requested from the Wi-Fi event loop; it closes the gate immediately. SPI task
owns the transaction and releases/reuses DMA buffers only after completion.

RX buffers belong to lwIP until its release callback. They are never reclaimed
on a session reset. TX queue entries carry a generation and DMA completions
carry their original local/peer session IDs. No socket/logging/large allocations
run in SPI ISR callbacks; those callbacks only change HS.

See the [wire contract](../../docs/spi-protocol.md) and
[maintenance guide](../../docs/firmware-maintenance.md). Protocol changes must
update both endpoints and the host tests together.
