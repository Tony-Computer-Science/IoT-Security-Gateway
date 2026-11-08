# tools/

Scripts that support development and evaluation, but are not part of any
board's firmware or the web interface.

| Path | Purpose |
|---|---|
| [test/](test/README.md) | Portable C protocol/policy tests, PC HTTP/API checks and a bounded TCP/UDP echo server |

Build scripts for the web interface live in `web/scripts/`, not here.

## Current owner (provisional)

Shared. Anyone may add tools. The project lead reviews.

## Conventions

- Python 3 standard library only, unless the team agrees on a dependency. If
  one is added, pin it in a `requirements.txt` next to the script that needs it.
- Keep credentials out of source. HTTP test harnesses generate temporary
  credentials and start their own loopback service; interactive services can
  read passwords from environment variables.
- Document script-specific arguments and prerequisites in [test/README.md](test/README.md).
  The C test runners need no base URL; the echo server takes `--bind`/`--port`.
