"""R6: an authorised client can poll status repeatedly."""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report

READS = 30
FIELDS = ("output", "target_reachable", "link_ok", "uptime_s", "seq")


def main():
    g = Gateway()
    r = Report("status_reads")
    g.reset()
    token = g.login()
    codes, last = [], {}
    for _ in range(READS):
        code, body = g.request("GET", "/api/status", token=token)
        codes.append(code)
        last = body
        if code == 429:
            time.sleep(1.0)
    ok = sum(1 for c in codes if c == 200)
    r.check("%d/%d reads succeed" % (ok, READS), ok == READS, "codes: %s" % codes[:5])
    r.check("every contract field is present", all(f in last for f in FIELDS), str(sorted(last.keys())))
    r.check("output is on or off", last.get("output") in ("on", "off"), str(last.get("output")))
    r.finish()


main()
