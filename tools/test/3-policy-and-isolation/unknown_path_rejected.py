"""R3: unknown paths are rejected (404 not_found)."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("unknown_path_rejected")
    g.reset()
    token = g.login()
    for path in ("/api/admin", "/api/../api/status", "/api/command/extra", "/api/"):
        code, body = g.request("GET", path, token=token)
        r.check("%s rejected" % path, code == 404, "got %s" % code)
        r.check("error is not_found", body.get("error") == "not_found", str(body))
    r.finish()


main()
