# tools/

Scripts that support development and evaluation, but are not part of any
board's firmware or the web interface.

| Path | Purpose |
|---|---|
| [test/](test/) | Evaluation and attack scripts for the test plan |

Build scripts for the web interface live in `web/scripts/`, not here.

## Current owner (provisional)

Shared. Anyone may add tools. The project lead reviews.

## Conventions

- Python 3 standard library only, unless the team agrees on a dependency. If
  one is added, pin it in a `requirements.txt` next to the script that needs it.
- Every script takes the gateway base URL as an argument (default:
  the mock server at `http://127.0.0.1:8000`) and never hardcodes credentials.
  Read them from environment variables.
