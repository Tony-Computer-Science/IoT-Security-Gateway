"""R2: a wrong password is refused and the attempt counter is reported."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import Gateway, Report


def main():
    g = Gateway()
    r = Report("bad_password_refused")
    g.reset()
    code, body = g.login_with("not-the-password")
    r.check("login is refused", code == 401, "got %s" % code)
    r.check("error is invalid_credentials", body.get("error") == "invalid_credentials", str(body))
    r.check("attempts_remaining is reported", isinstance(body.get("attempts_remaining"), int), str(body.get("attempts_remaining")))
    r.check("no token is issued", "token" not in body, str(sorted(body)))
    r.finish()


main()
