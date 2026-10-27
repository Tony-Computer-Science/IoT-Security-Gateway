# Board B: Wi-Fi uplink and IPv4 router

ESP32-S3, ESP-IDF **6.1**, at least 4 MB flash, no PSRAM requirement. B joins
existing 2.4 GHz WPA2/WPA3 personal Wi-Fi and NATs IPv4 packets from Board A's
SPI address. It creates no access point and serves no HTTP management API.

## Components

| Component | Responsibility |
|---|---|
| uplink_wifi | Station-only Wi-Fi, DHCP, 1–30 second reconnect backoff, 30 second initial DHCP timeout, state snapshot |
| bridge_router | Readiness supervisor, NAPT, overlap checks, IPv4 ingress/forwarding hooks |
| ../common/gateway_link | SPI endpoint, virtual Ethernet netif, bounded queues/pools, reset handling |
| ../common/gateway_protocol | Portable codec, sessions, status encoding and packet policy |

The previous ap_mode, spi_device and target_client stubs were removed. The
test host uses the same shared link component compiled as the controller.

## Configure and build

Load ESP-IDF 6.1 first. On this workstation, the Installation Manager supplies:

```sh
. /home/tonyg/.espressif/tools/activate_idf_v6.1.sh
cd /home/tonyg/Documents/College/IoT-Security-Gateway/firmware/bridge-board
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Use the actual serial port for B. Only run `set-target` on initial setup or
target changes: it resets configuration. In **Component config → Bridge
upstream Wi-Fi**, enter your SSID (1–32 bytes) and passphrase (8–63 bytes).
Empty defaults deliberately build but leave forwarding disabled with a
configuration-error status. No credentials are requested in chat or committed.
Open/WEP/enterprise/captive-portal networks are unsupported. Configuration
is compiled into firmware; generated binaries, sdkconfig and sdkconfig.old
must remain private. Wi-Fi runtime storage is RAM, not NVS.

**Gateway SPI link** settings must match the test/production host. Default
B=192.168.77.1, A=192.168.77.2, /30. Change both if the upstream LAN overlaps.
Board B must have the controller option disabled. A 4 MB flash setting is a
conservative image size for larger ESP32-S3 modules as well. NVS initialization
errors stop startup rather than silently erase stored data.

If invoking idf.py directly instead of using EIM activation, set IDF_PATH,
IDF_TOOLS_PATH, IDF_PYTHON_ENV_PATH and ESP_IDF_VERSION and add the EIM compiler,
CMake, Ninja and Python directories to PATH. The EIM Python environment is
`~/.espressif/tools/python/v6.1/venv`, not IDF's conventional python_env path.

## Wiring and operation

Connect the same GPIO number on each board: SCLK=12, MOSI=11, MISO=13, CS=10,
HS=4, plus common GND. See [pinout](../../hardware/docs/README.md). No extra
handshake or reset wire is required. A controls the 4 MHz clock; B uses mode 0.
HS means transaction ready. Do not connect the boards' USB data paths together.

Serial output distinguishes association, DHCP readiness, disconnect reason,
retry delay and forwarding state. IP readiness is not proof of working internet.
SPI STATUS carries addresses, DNS, configuration/overlap flags and counters.
Loss of connectivity closes the packet path and clears NAT; old connections
must reconnect. See [protocol](../../docs/spi-protocol.md) for exact framing.

## Validation and limits

Run `python3 tools/test/run_host_tests.py` from the repository root, then use
the [test-host image](../spi-test-host/README.md) and [hardware checklist](../../docs/testing.md).
Compilation and host tests do not validate SPI electrical timing, Wi-Fi
association, actual NAT behavior or throughput. Those checks need both boards.

IPv4 only; outbound TCP/UDP/ICMP, no port forwarding, no VLAN, no IP fragments.
Large fragmented UDP and protocols requiring inbound connections are outside
this milestone. [Production A](../station-board/README.md) provides WPA2 AP,
client DHCP and NAT; application security policy remains future work.
The original command-only requirements are incompatible
with this router; see [architecture](../../docs/architecture.md).
