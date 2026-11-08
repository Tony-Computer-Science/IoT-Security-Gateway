# Board B: Wi-Fi uplink and IPv4 router

ESP32-S3 or ESP32-C6, ESP-IDF **6.1**, at least 4 MB flash, no PSRAM requirement.
B joins
existing 2.4 GHz WPA2/WPA3 personal Wi-Fi and NATs IPv4 packets from Board A's
SPI address. It creates no access point and serves no HTTP management API.

On C6-WROOM-1 DevKitC-1 boards, the onboard GPIO8 RGB LED shows **red**
to identify this firmware role. This is a fixed identity color, not a network
readiness indicator. Configure or disable it under **Board identity LED**.

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
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Use the actual serial port for B. The example selects C6; use
`idf.py set-target esp32s3` for the Rev A PCB/S3 boards. A fresh checkout still
defaults to S3. Only run `set-target` on initial setup or target changes:
it resets configuration, including Wi-Fi settings. Preserve private settings
beforehand and restore them in menuconfig. In **Component config → Bridge
upstream Wi-Fi**, enter your SSID (1–32 bytes) and passphrase (8–63 bytes).
Empty defaults deliberately build but leave forwarding disabled with a
configuration-error status. No credentials are requested in chat or committed.
Open/WEP/enterprise/captive-portal networks are unsupported. Configuration
is compiled into firmware; generated binaries, sdkconfig and sdkconfig.old
must remain private. Wi-Fi runtime storage is RAM, not NVS.

**Gateway board link** settings must match the test/production host. Default
B=192.168.77.1, A=192.168.77.2, /30. Change both if the upstream LAN overlaps.
Board B must have the controller option disabled. A 4 MB flash setting is a
conservative image size for larger ESP32-S3 and ESP32-C6 modules as well.
NVS initialization errors stop startup rather than silently erase stored data.

If invoking idf.py directly instead of using EIM activation, set IDF_PATH,
IDF_TOOLS_PATH, IDF_PYTHON_ENV_PATH and ESP_IDF_VERSION and add the EIM compiler,
CMake, Ninja and Python directories to PATH. The EIM Python environment is
`~/.espressif/tools/python/v6.1/venv`, not IDF's conventional python_env path.

### Target change and flashing troubleshooting

- `Private include directory ... wpa_supplicant ... esp_wifi` is a CMake
  warning from ESP-IDF 6.1. Check the later `CMake Error` for the actual cause;
  it does not require editing the installed SDK.
- Both projects support `esp32s3` and `esp32c6`. The old
  `This board pinout requires ESP32-S3` check prevented C6 configuration.
- If sdkconfig already selects C6 but the build cache selects S3, preserve
  sdkconfig and move `build/` into ignored `local-backups/`, then build again.
- In VS Code choose **ESP-IDF: Select Flash Method → UART** for serial flashing
  through the selected `/dev/ttyACM*` or `/dev/ttyUSB*` port, including native
  USB Serial/JTAG. The JTAG flash method uses OpenOCD instead of the serial
  port. Close any existing serial monitor before flashing.
- If a C6 board does not connect, hold BOOT, tap RESET/EN, release BOOT and
  retry `idf.py -p <port> -b 115200 flash`. Recheck the port after reset. Use a
  USB data cable. If old firmware used GPIO12/13 for SPI, disconnect those
  wires before recovering native USB.
- Exit `idf.py monitor` with Ctrl+]. Flash each board on its own actual port.
- If compiler processes are killed during a build, limit parallel jobs with
  `IDF_PY_BUILD_JOBS=2 idf.py build` to reduce memory use.

## Two-wire UART test option

For tests with only two signal jumpers and an existing common ground, enable
**Component config → Gateway board link → Use two-wire UART instead of SPI**
on **both** boards. Keep Board A endpoint enabled on station and disabled on
bridge. Default settings: UART1, 921600 baud, 8-N-1, no hardware flow control.

- Station GPIO6 (TX) → bridge GPIO7 (RX).
- Bridge GPIO6 (TX) → station GPIO7 (RX).
- Keep GND connected; power and flash through each board's USB cable.

Rebuild and flash both images. This mode does not use SCLK, CS or HS wires.
The existing /30 addresses, sessions, status, packet checks, DHCP and NAT
remain in use. Throughput is much lower than SPI; use it for basic connectivity
and recovery checks. It does not validate SPI wiring or performance. See the
[UART envelope and test procedure](../../docs/uart-link.md).

SPI remains the tracked default. To return to it, disable the UART option on
both endpoints and restore the target-specific SPI wiring.

## Wiring and operation

Default C6 wiring: SCLK=6, MOSI=7, MISO=2, CS=18, HS=19, plus common GND.
S3 retains SCLK=12, MOSI=11, MISO=13, CS=10, HS=4. See the
[target-specific pinout](../../hardware/docs/README.md). Match corresponding
signals on the boards and verify **Gateway board link** pin settings against
your board's headers. C6 GPIO12/13 must remain free for native USB. No extra
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

New upstream TCP SYN packets without ACK are rejected before NAT. Replies to
client-initiated connections remain allowed; no inbound port mapping is
enabled. See [client-network boundaries](../../docs/client-network.md).
