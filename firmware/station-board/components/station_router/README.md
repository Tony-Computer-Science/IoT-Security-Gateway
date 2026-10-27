# station_router

Owns A's SPI netif and client NAPT lifecycle. A 50 ms supervisor validates B's
current status with the portable `gateway_access` policy, propagates DNS and
enables NAPT on the **AP interface** before opening the SPI data gate. The
default route is SPI; clients are translated to A's SPI IP before B translates
them again to its upstream address.

On loss: close the data gate, clear AP NAPT while its netif is up, then bring
the SPI netif down. AP/DHCP stays available. If the AP itself unexpectedly
stops, retain pending NAT ownership until it is up and can be cleared; never
reopen first. Setup failures retry at most once per second.

The SDK-supported hooks run on tcpip_thread and recheck readiness for queued
packets. Local DHCP and gateway ping are allowed while offline. Client source
addresses must belong to the AP subnet; pre-NAT SPI return packets must target
A's SPI address. Direct routes to AP clients cannot bypass NAT. IPv4/L4 shape
validation precedes NAPT parsing. No SDK source files are modified.

This is routing policy, not application authorization or layer-2 client
isolation. The original auth/policy/eventlog placeholders are not invoked.
