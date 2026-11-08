"""R3: a known path with the wrong method is rejected (405)."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("wrong_method_rejected")
    g.reset()
    token = g.login()
    for method, path in (("PUT", "/api/status"), ("DELETE", "/api/status"),
                         ("GET", "/api/command"), ("PATCH", "/api/login")):
        code, body = g.request(method, path, token=token, raw=b"{}" if method != "GET" else None)
        r.check("%s %s rejected" % (method, path), code == 405, "got %s" % code)
        r.check("error is method_not_allowed", body.get("error") == "method_not_allowed", str(body))
    r.finish()


main()
