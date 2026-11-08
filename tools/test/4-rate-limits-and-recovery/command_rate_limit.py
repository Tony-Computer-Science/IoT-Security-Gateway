"""R4: the command rate limit stops a burst and reports retry_after."""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report

FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "..", "..", "web", "mock", "fixtures.json")


def main():
    g = Gateway()
    r = Report("command_rate_limit")
    g.reset()
    with open(FIXTURES, encoding="utf-8") as fh:
        allowed = json.load(fh)["command_rate_limit"]["max"]
    token = g.login()

    codes = []
    for _ in range(allowed + 3):
        code, body = g.request("POST", "/api/command",
                               {"action": "led_on" if len(codes) % 2 == 0 else "led_off"},
                               token=token)
        codes.append(code)
        if code == 429:
            r.check("error is rate_limited", body.get("error") == "rate_limited", str(body))
            r.check("retry_after is a positive number",
                    isinstance(body.get("retry_after"), (int, float)) and body["retry_after"] > 0,
                    str(body.get("retry_after")))
            break

    r.check("first %d commands are accepted" % allowed, codes[:allowed] == [200] * allowed, str(codes[:allowed]))
    r.check("a burst is eventually rate limited", 429 in codes, str(codes))
    r.finish()


main()
