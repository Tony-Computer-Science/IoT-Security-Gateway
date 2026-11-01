# Web events view: how it works

Code: the "Events view" and "Status polling" sections of `web/src/app.js`.
Contract: `GET /api/events` in `docs/api-contract.md`.

## What the log records and why

Requirement R5 says the gateway keeps a bounded log of security-relevant
events, and that only signed-in users can read it. Each entry has:

- a sequence number, which counts up and is never reused while the gateway
  is running,
- the gateway's uptime when it happened,
- a type: sign-in (`auth`), a device command (`command`), a rate limit
  (`rate_limit`), or a problem reaching the device (`target`),
- an outcome: `allowed`, `denied`, or `alert`,
- a short description, which never contains a password or session token.

This is how an operator sees attacks happening, for example a run of failed
sign-ins followed by a lockout. "Bounded" means the log has a fixed size, so
the oldest entries drop off. An attacker therefore cannot fill the board's
memory by generating events.

The newest entry is shown first. Each outcome reuses a status style from the
control view: allowed has a green ✓, denied has a red ✕, and alerts take the
style of what caused them (‖ for a rate limit, ⚠ for an unreachable device).
They differ in symbol and border, not only in color.

## Why filtering happens on the gateway

When the user picks "Denied", the page asks the gateway for
`/api/events?outcome=denied`. It does not download everything and hide rows
in the browser. Two reasons:

1. The gateway returns only a limited number of entries per request. If the
   browser filtered, an older denied entry could be missing simply because
   newer allowed entries filled that limit. Asking the gateway for denied
   entries gets the newest denied entries it actually holds.
2. What the screen shows is exactly what the gateway reports. In testing,
   every filter returned the same entries as the same request made with curl.

## Sharing the polling loop with the control view

The app has a single polling loop. It has one timer and allows only one
request at a time. When the control view is open, the loop's job is "fetch
status". When the events view is open, its job is "fetch events". Leaving a
view stops its job, and opening the other view hands the loop the new job.

Because there is only one loop, an events refresh and a status poll can never
run at the same time. If the user switches views while a request is still
in flight, the new view's first refresh waits until that request finishes,
then runs straight away. A late answer from the view you just left is
ignored. Changing the filter or pressing Refresh asks the same loop for an
immediate run, so it follows the same rules.

## Keeping the list readable during a live demo

- **Nothing new:** if a refresh returns the same entries, the page is not
  touched at all.
- **Reading further down:** before redrawing, the page notes which row is at
  the top of the screen. Afterwards it scrolls so that row is in the same
  place. New entries appear above without moving what you are reading.
- **At the top of the page:** new entries simply appear at the top.
- **Refresh fails:** the last loaded list stays, and a message beside it says
  the gateway could not be reached. A 401 sends the user back to sign in.
- **Nothing recorded:** a message says no events have been recorded yet,
  instead of an empty panel.
