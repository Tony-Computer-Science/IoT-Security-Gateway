# uplink_wifi

Station-only WPA2/WPA3 personal uplink. Credentials come from menuconfig and
are never logged. Runtime Wi-Fi config stays in RAM. Association, DHCP, retries
and errors are distinct status fields. A 30-second DHCP watchdog disconnects
an association that never receives an address. Reconnect delays grow from
1 to 30 seconds; acquiring IPv4 resets the delay. Timers post to the default
event loop so retry and connection events cannot race timer reconfiguration.

Uplink loss/address changes invalidate the SPI session immediately. The router
supervisor owns NAT lifecycle. `uplink_wifi_status` returns a synchronized
snapshot; `uplink_wifi_ready` means IPv4 acquired, not internet verified.
