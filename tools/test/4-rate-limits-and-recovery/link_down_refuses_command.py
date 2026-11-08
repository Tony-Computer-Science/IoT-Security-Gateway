"""R7: while the SPI link is down, commands are refused with 503."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("link_down_refuses_command")
    g.reset()
    token = g.login()
    before = g.output(token)

    g.inject(link_ok=False)
    code, body = g.request("POST", "/api/command", {"action": "led_on"}, token=token)
    r.check("command returns 503", code == 503, "got %s" % code)
    r.check("error is link_down", body.get("error") == "link_down", str(body))
    r.check("target state did not change", g.output(token) == before, "%s -> %s" % (before, g.output(token)))

    code, body = g.request("GET", "/api/status", token=token)
    r.check("status reports link_ok false", body.get("link_ok") is False, str(body.get("link_ok")))
    g.reset()
    r.finish()


main()
