"""R7: once the link is back, commands work again without a restart."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("recovery_after_link_restore")
    g.reset()
    token = g.login()

    g.inject(link_ok=False)
    r.check("command refused while the link is down",
            g.request("POST", "/api/command", {"action": "led_on"}, token=token)[0] == 503)

    g.inject(link_ok=True)
    code, body = g.request("POST", "/api/command", {"action": "led_on"}, token=token)
    r.check("command accepted after the link is restored", code == 200, "got %s" % code)
    r.check("target reflects the command", g.output(token) == "on", str(g.output(token)))
    g.reset()
    r.finish()


main()
