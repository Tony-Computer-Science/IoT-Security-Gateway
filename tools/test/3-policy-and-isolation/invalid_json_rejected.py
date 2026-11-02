"""R3: a body that is not valid JSON is rejected (400 invalid_request)."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("invalid_json_rejected")
    g.reset()
    token = g.login()
    for raw in (b"{not json", b"[1,2,3]", b"", b"null"):
        code, body = g.request("POST", "/api/command", token=token, raw=raw)
        r.check("body %r rejected" % raw[:12], code == 400, "got %s" % code)
        r.check("error is invalid_request", body.get("error") == "invalid_request", str(body))
    r.check("target state did not change", g.output(token) == "off", str(g.output(token)))
    r.finish()


main()
