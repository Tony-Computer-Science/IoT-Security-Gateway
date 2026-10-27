# bridge_router

Supervises the SPI netif/NAPT every 50 ms. Data forwarding requires a valid
peer session, upstream IPv4 and non-overlapping subnets. NAT is enabled before
the data gate opens and disabled before the netif goes down. The shared
transport and Wi-Fi event handlers close the data gate immediately on loss.

The project's top-level CMake installs gateway_lwip_hooks.h into the SDK's
supported additional-hook mechanism. Hooks gate queued packets, reject spoofed
SPI source addresses and prevent direct upstream routes into the SPI subnet.
They also validate L3/L4 lengths and reject fragments before NAPT parses them.
Port mapping and IPv6 are disabled. No SDK source files are modified.

The supervisor also samples current upstream DNS settings when publishing
STATUS. A DHCP renewal that changes DNS without changing the IP address must
still reach production A so its client DHCP options can be refreshed.
