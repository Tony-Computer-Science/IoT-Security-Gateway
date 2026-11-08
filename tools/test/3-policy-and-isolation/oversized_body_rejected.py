"""R3: a body larger than the server limit is rejected (413)."""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report

FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "..", "..", "web", "mock", "fixtures.json")


def main():
    g = Gateway()
    r = Report("oversized_body_rejected")
    g.reset()
    token = g.login()
    with open(FIXTURES, encoding="utf-8") as fh:
        limit = json.load(fh)["max_body_bytes"]

    for size in (limit + 1, limit * 4):
        raw = json.dumps({"action": "led_on", "pad": "x" * size}).encode()
        code, body = g.request("POST", "/api/command", token=token, raw=raw)
        r.check("%d byte body rejected" % len(raw), code == 413, "got %s" % code)
        r.check("error is payload_too_large", body.get("error") == "payload_too_large", str(body))
    r.check("target state did not change", g.output(token) == "off", str(g.output(token)))
    r.finish()


main()
