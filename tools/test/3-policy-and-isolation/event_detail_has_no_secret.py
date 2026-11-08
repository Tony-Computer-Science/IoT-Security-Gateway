"""R2: the event log never echoes the password or a session token."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report, credentials


def main():
    g = Gateway()
    r = Report("event_detail_has_no_secret")
    g.reset()
    user, password = credentials()
    token = g.login()
    g.login_with("wrong-password-for-the-log")
    g.request("POST", "/api/command", {"action": "reboot"}, token=token)

    code, body = g.request("GET", "/api/events", token=token)
    r.check("events endpoint returns 200", code == 200, "got %s" % code)
    events = body.get("events", [])
    r.check("at least one event was recorded", len(events) > 0, "%d events" % len(events))
    details = " ".join(str(e.get("detail", "")) for e in events)
    r.check("password not in any detail", password not in details)
    r.check("token not in any detail", token not in details)
    r.check("wrong password not echoed", "wrong-password-for-the-log" not in details)
    r.finish()


main()
