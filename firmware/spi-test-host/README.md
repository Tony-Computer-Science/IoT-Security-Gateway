# Board A SPI test host

Separate ESP-IDF 6.1 image for ESP32-S3. It never initializes Wi-Fi: all network
tests must pass through SPI and Board B. This is not production Board A firmware.

Load your ESP-IDF 6.1 environment, then:

```sh
cd firmware/spi-test-host
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyACM1 flash monitor
```

Choose the actual A port. Keep **Gateway board link → Build Board A endpoint**
enabled, the clock at 4 MHz and the address settings identical to B. Wiring:
SCLK 12, MOSI 11, MISO 13, CS 10, HS 4 on both ends, plus common ground.

Under **SPI host network tests**, configure DNS name, HTTPS URL, NTP server,
optional echo server address/port and test interval (30 seconds by default).
DNS/HTTPS defaults use example.com. HTTPS uses a HEAD request, verifies the
server hostname and certificate chain with ESP-IDF's root bundle and requires
a clock synchronized via SNTP. A missing clock reports SKIP, never disables
certificate checks. Redirects are not followed. HTTP 200–399 counts as an
authenticated response; an HTTP error or TLS failure reports FAIL.

For echo tests, run on a PC on your controlled upstream LAN:

```sh
python3 tools/test/echo_server.py --bind <PC-IPv4-address> --port 9000
```

Set that address in the host's menuconfig. The server listens for TCP and UDP
on the selected interface. Restrict it to your lab network. The host reports
PASS/FAIL for byte-exact echo; an empty echo address explicitly reports SKIP.
TCP and UDP echo workers run concurrently with HTTPS. Each echo payload is
1472 bytes: UDP exercises the 1500-byte IP MTU and TCP spans segments.

Tests repeat while the relay is available and report free/minimum internal
heap. Leave running for at least 30 minutes for the soak check. The supervisor
validates B's STATUS addressing, installs its DNS servers and closes the data
interface when the peer or upstream is unavailable. No external request is
made until forwarding is reported ready.

Read [hardware acceptance](../../docs/testing.md) before interpreting logs:
a successful compilation is not a successful hardware test. Restore the
production Board A image when moving beyond this isolated test application.
