"""R2: logout invalidates the token, so a replay is refused."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("token_replay_after_logout")
    g.reset()
    token = g.login()
    r.check("token works before logout", g.request("GET", "/api/status", token=token)[0] == 200)

    code, _ = g.request("POST", "/api/logout", token=token)
    r.check("logout returns 204", code == 204, "got %s" % code)

    code, body = g.request("GET", "/api/status", token=token)
    r.check("replayed token is refused", code == 401, "got %s" % code)
    r.check("error is invalid_session", body.get("error") == "invalid_session", str(body))

    code, _ = g.request("POST", "/api/logout", token=token)
    r.check("second logout is refused", code == 401, "got %s" % code)
    r.finish()


main()
