"""R2: a token the gateway never issued is refused."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("forged_token_refused")
    g.reset()
    g.login()
    for fake in ("0" * 43, "deadbeef", "../../etc/passwd"):
        code, body = g.request("GET", "/api/status", token=fake)
        r.check("token %r refused" % fake[:12], code == 401, "got %s" % code)
        r.check("error is invalid_session", body.get("error") == "invalid_session", str(body))
    r.finish()


main()
