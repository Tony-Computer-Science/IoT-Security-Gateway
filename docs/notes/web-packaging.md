# Web packaging and PC hosting

The website and APIs run on the **fixed-IP PC**, which acts as the server at
**http://192.168.78.10:8000/**. No ESP32 mounts or serves UI assets in this
setup. Board C polls that endpoint using its compiled-in address and port.

## Current setup

1. Configure the PC as 192.168.78.10/24 on IoT-Gateway using the
   [client-network guide](../client-network.md).
2. From the repository root, run:

   ```sh
   python3 web/device_server.py --host 192.168.78.10 --port 8000
   ```

3. Open `http://192.168.78.10:8000/`. The service serves `web/src/` directly
   and handles the browser API and device polling on the same port.

No asset build or image flashing is needed to serve the UI. A dedicated
server can replace the PC later; Board C must be rebuilt and flashed if
its configured server address/port changes.

## Asset checks and retained legacy tooling

`python3 web/scripts/build.py` minifies and gzips the browser files into
`web/dist/` and checks forbidden APIs/external URLs and the 60 KiB size budget.
This remains a frontend check, not an ESP32 deployment step.

The unused `www` partition, `mklittlefs.sh` packer and `verify_littlefs.py`
verifier are retained from the former onboard-hosting design. They do not
imply current or planned board hosting. The verifier can inspect a historical
image with the optional pinned image-tool dependency; neither it nor that
image is used by the PC service. See [build tooling](../../web/scripts/README.md).
