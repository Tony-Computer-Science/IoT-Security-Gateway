"""R3: an action outside the allow-list is rejected and not forwarded."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report

BAD = ["reboot", "LED_ON", "", "led_on; reboot", "../../led", "l" * 300, None, 1, ["led_on"]]


def main():
    g = Gateway()
    r = Report("invalid_action_rejected")
    g.reset()
    token = g.login()
    before = g.output(token)
    for action in BAD:
        code, body = g.request("POST", "/api/command", {"action": action}, token=token)
        r.check("action %r rejected" % (action if not isinstance(action, str) else action[:12]),
                code == 400, "got %s" % code)
        r.check("error is invalid_action", body.get("error") == "invalid_action", str(body))
    r.check("target state did not change", g.output(token) == before, "%s -> %s" % (before, g.output(token)))
    r.finish()


main()
