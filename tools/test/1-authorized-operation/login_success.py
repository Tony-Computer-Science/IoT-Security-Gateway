"""R2: a provisioned credential is exchanged for a session token."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report, credentials


def main():
    g = Gateway()
    r = Report("login_success")
    g.reset()
    user, password = credentials()
    code, body = g.request("POST", "/api/login",
                           {"username": user, "password": password})
    r.check("login returns 200", code == 200, "got %s" % code)
    r.check("a token is issued", isinstance(body.get("token"), str) and body["token"], str(body)[:80])
    r.check("expires_in is a number", isinstance(body.get("expires_in"), (int, float)), str(body.get("expires_in")))
    r.finish()


main()
