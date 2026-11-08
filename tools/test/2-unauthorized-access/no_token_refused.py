"""R2: a status read without a bearer token is refused and leaks nothing."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("no_token_refused")
    g.reset()
    code, body = g.request("GET", "/api/status")
    r.check("status read is refused", code == 401, "got %s" % code)
    r.check("error is invalid_session", body.get("error") == "invalid_session", str(body))
    r.check("no device state leaks", not set(body) & {"output", "uptime_s", "link_ok"}, str(sorted(body)))
    r.finish()


main()
