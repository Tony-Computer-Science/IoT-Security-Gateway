# web/scripts: web build tooling

Named `scripts/`, **not** `build/`. ESP-IDF's ignore pattern `build/` matches at
any depth and would hide a `web/build/` directory from git.

| Script | What it does |
|---|---|
| `build.py` | Minifies `web/src` (pure Python), gzips each file into `web/dist/*.gz`, prints a size report, and fails if the gzipped total exceeds 60 KB or a forbidden API or external URL is used. `--keep-min` also writes the uncompressed minified files. |
| `mklittlefs.sh` | Packs `web/dist/` into a LittleFS image sized from the `www` row of `firmware/station-board/partitions.csv`. Needs the `mklittlefs` tool. |

```sh
python3 web/scripts/build.py
web/scripts/mklittlefs.sh            # -> web/littlefs.bin (git-ignored)
```

Note: `web/src/` deliberately has no README. Every file in `web/src/` is
shipped to the board's flash. See [../README.md](../README.md) instead.

## Current owner (provisional)

Lalit Dhakal. Any member may work here; the current owner reviews.
