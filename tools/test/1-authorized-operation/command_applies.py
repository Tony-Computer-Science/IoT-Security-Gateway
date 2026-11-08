"""R6: an authorised command changes the target and status reflects it."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("command_applies")
    g.reset()
    token = g.login()

    code, body = g.request("POST", "/api/command", {"action": "led_on"}, token=token)
    r.check("led_on returns 200", code == 200, "got %s" % code)
    r.check("result is ok and output is on", body.get("result") == "ok" and body.get("output") == "on", str(body))
    r.check("status reports the new state", g.output(token) == "on", str(g.output(token)))

    code, body = g.request("POST", "/api/command", {"action": "led_off"}, token=token)
    r.check("led_off returns 200", code == 200, "got %s" % code)
    r.check("status reports off again", g.output(token) == "off", str(g.output(token)))
    r.finish()


main()
