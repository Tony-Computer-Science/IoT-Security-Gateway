# IoT Security Gateway

Repository: `Tony-Computer-Science/IoT-Security-Gateway`.
Read `CONTRIBUTING.md` for branch naming, validation and required owner reviews.
Use topic branches and PRs; merge only after the required reviews and checks.

For the real-device UI and S3 test, read `docs/s3-live-test.md` before building
or flashing. This branch combines main's frontend with the target/backend from
`codex/spi-wifi-router`; it does not import the A/B router implementation.
Preserve the existing login/control/events behavior. The browser uses plain
JavaScript and a Python asset build, with no runtime framework or dependencies.
Keep Wi-Fi credentials, sdkconfig, firmware containing credentials and full-flash
backups private. Confirm the carrier's LED pin before flashing an S3.

Run `bash ci/run.sh` for portable tests and asset validation. Build each affected
chip target with ESP-IDF 6.1 and run `python3 tools/test/run_target_tests.py`
after managed dependencies are installed. Software tests do not establish
hardware or gateway-isolation acceptance; record those observations separately.
