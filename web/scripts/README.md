# web/scripts: web build tooling

Named `scripts/`, **not** `build/`. ESP-IDF's ignore pattern `build/` matches at
any depth and would hide a `web/build/` directory from git.

| Script | What it does |
|---|---|
| `build.py` | Minifies `web/src` (pure Python), gzips each file into `web/dist/*.gz`, prints a size report, and fails if the gzipped total exceeds 60 KB or a forbidden API or external URL is used. `--keep-min` also writes the uncompressed minified files. |
| `verify_littlefs.py` | Read-only legacy image verifier against `web/dist/`; optional `requirements-image.txt` dependency, unused by PC hosting. |
| `mklittlefs.sh` | Legacy packer from the former onboard-hosting design. Packs `web/dist/` using the retained `www` partition size; requires `mklittlefs`. Unused by the separate web service. |

```sh
python3 web/scripts/build.py
```

The current service serves `web/src/` directly. It requires neither this
build nor an asset image flashed to an ESP32. `web/src/` deliberately contains
only browser assets; put documentation in [../README.md](../README.md).
See [Setting Up](../../README.md#setting-up) for the local-computer demo.

## Current owner (provisional)

Lalit Dhakal. Any member may work here; the current owner reviews.
