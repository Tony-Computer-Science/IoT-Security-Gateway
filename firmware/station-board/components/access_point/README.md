# access_point

Owns AP-only WPA2/AES configuration and DHCP. `access_point_start()` validates
credentials and a private .1/24 distinct from SPI before initializing Wi-Fi.
No empty-password fallback exists. Runtime Wi-Fi configuration is RAM-only.

The Wi-Fi event loop owns the synchronized running flag. The router supervisor
alone updates DHCP DNS options after startup. Addresses are immutable; public
headers specify which APIs use host-order versus network-order integers.
DNS changes stop/reconfigure/start DHCP, then deauthenticate clients so their
next association refreshes their lease. Ordinary upstream loss leaves the AP
and last DHCP options available. No payload or credential logging is used.

Configuration is under **Station board WPA2 access point** in menuconfig.
See [board setup](../../README.md) for defaults, DNS behavior and limitations.
