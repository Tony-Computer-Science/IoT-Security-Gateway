"""R6: when Board B cannot reach the target, commands return 502."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("target_unreachable_refuses_command")
    g.reset()
    token = g.login()
    before = g.output(token)

    g.inject(target_reachable=False)
    code, body = g.request("POST", "/api/command", {"action": "led_on"}, token=token)
    r.check("command returns 502", code == 502, "got %s" % code)
    r.check("error is target_unreachable", body.get("error") == "target_unreachable", str(body))
    r.check("target state did not change", g.output(token) == before, "%s -> %s" % (before, g.output(token)))
    g.reset()
    r.finish()


main()
