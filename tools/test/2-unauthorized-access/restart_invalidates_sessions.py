"""R7: restarting the gateway invalidates outstanding sessions."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("restart_invalidates_sessions")
    g.reset()
    token = g.login()
    r.check("session is valid before restart",
            g.request("GET", "/api/status", token=token)[0] == 200)

    g.reset()

    code, body = g.request("GET", "/api/status", token=token)
    r.check("old token is refused after restart", code == 401, "got %s" % code)
    r.check("error is invalid_session", body.get("error") == "invalid_session", str(body))

    code, body = g.request("POST", "/api/command", {"action": "led_on"}, token=token)
    r.check("an old token cannot command the target", code == 401, "got %s" % code)
    r.finish()


main()
