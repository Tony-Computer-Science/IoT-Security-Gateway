# IoT Security Gateway

Repository: `Tony-Computer-Science/IoT-Security-Gateway`.
Read `CONTRIBUTING.md` for branch naming, validation and required owner reviews.
Use topic branches and PRs; merge only after the required reviews and checks.

For the real-device UI and S3 test, read `docs/s3-live-test.md` before building
or flashing. The repository combines the A/B SPI/UART router, the polling
target/backend and main's login/control/events frontend with S3/C6 target support.
The UI/API runs on the fixed-IP PC at 192.168.78.10:8000, never on an ESP32.
Preserve the existing login/control/events behavior. The browser uses plain
JavaScript and a Python asset build, with no runtime framework or dependencies.
Keep Wi-Fi credentials, sdkconfig, firmware containing credentials and full-flash
backups private. Confirm the carrier's LED pin before flashing an S3.

Run `bash ci/run.sh` for portable tests and asset validation: service tests,
browser client tests (Node 18+), the firmware parser with sanitizers (it fetches
pinned cJSON if no ESP-IDF build has installed it) and the web build. Build each
affected chip target with ESP-IDF 6.1. Software tests do not establish
hardware or gateway-isolation acceptance; record those observations separately.
