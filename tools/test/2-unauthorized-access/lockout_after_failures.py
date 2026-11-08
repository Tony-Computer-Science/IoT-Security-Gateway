"""R4: repeated failed logins lock the account and report retry_after."""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report

FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "..", "..", "web", "mock", "fixtures.json")


def main():
    g = Gateway()
    r = Report("lockout_after_failures")
    g.reset()
    with open(FIXTURES, encoding="utf-8") as fh:
        limit = json.load(fh)["max_failed_logins"]

    codes, bodies = [], []
    for _ in range(limit):
        code, body = g.login_with("wrong")
        codes.append(code)
        bodies.append(body)
    r.check("first %d attempts are 401" % limit, codes == [401] * limit, str(codes))
    r.check("last attempt reports attempts_remaining=0",
            bodies[-1].get("attempts_remaining") == 0, str(bodies[-1]))

    code, body = g.login_with("wrong")
    r.check("further attempts are 429", code == 429, "got %s" % code)
    r.check("error is locked_out", body.get("error") == "locked_out", str(body))
    r.check("retry_after is a positive number",
            isinstance(body.get("retry_after"), (int, float)) and body["retry_after"] > 0,
            str(body.get("retry_after")))

    code, _ = g.request("POST", "/api/login",
                        {"username": "someone-else", "password": "x"})
    r.check("lockout is not bypassed by another username", code == 429, "got %s" % code)
    r.finish()


main()
