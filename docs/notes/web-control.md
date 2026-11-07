# Web control view: how it works

Code: the "Control view" and "Status polling" sections of `web/src/app.js`.
Contract: `GET /api/status` and `POST /api/command` in `docs/api-contract.md`.

## What the user sees

Three status rows are always visible: the LED output (On or Off), whether the
target device is reachable, and whether the board-to-board link is up. Below
them are two buttons, "Turn on" and "Turn off", and two message lines. The
first message line shows the gateway's health. The second shows what happened
to the user's last click.

## Why there are no optimistic updates

Many apps change the screen the moment you click, then fix it if the request
fails. This view does not. The LED row changes only when the gateway answers
with the target's real state: a successful command response, or a status
update. While a command is on its way, the view shows a "Sending" message and
the LED row keeps its old value.

Without this rule, here is what an R7 failure would look like. The user clicks
"Turn on". The screen immediately says "On". But the link between the two
gateway boards is down, so the command never reaches the device and the real
LED stays off. If the user looked away before the error appeared, they would
believe the light is on. R7 says a failure must never look like a success, and
R6 says a rejected request leaves the device unchanged. So the screen must
stay unchanged too.

## Link down versus target unreachable

These are two different failures in two different places, and they look
different on screen.

The hardware runs in this order: the bridge board (Board B) joins the
upstream Wi-Fi, a UART wire connects it to the station board (Board A), and A
creates the private Wi-Fi "IoT-Gateway" that the IoT device (Board C) and the
operator's laptop join. The diagram on the page reads in the same order:
B, then A, then C.

- **Link down** (hatched box, broken chain link icon): the bridge board and
  the station board cannot talk over the UART wire.
- **Target unreachable** (dotted box, Wi-Fi with a slash): the gateway boards
  are talking fine, but the station board cannot reach the IoT device on its
  private Wi-Fi.

In both cases the buttons are disabled, and the message names which part
failed. If both are reported, link down is shown first.

## How the polling loop avoids overlap

The page asks for status every 2 seconds. It never has two requests open at
once:

1. Only one timer exists at a time. Setting a new one cancels the old one.
2. The next poll is scheduled only after the current one finishes. A slow
   gateway makes polling slower instead of piling up requests.
3. If a timer fires while a poll or a command is still in flight, that tick
   is skipped.
4. When the user clicks a button, polling pauses. If a poll was already in
   flight, the command waits for it to finish before it is sent. Polling
   resumes when the command finishes.

Testing against the mock server with slow responses showed at most one
request open at any moment.

## What happens when a poll fails

A failed poll does not blank the screen. The last known LED state, link state,
and target state stay visible, and the health line says "Could not reach the
gateway. Showing the last known state." The next successful poll replaces the
message with fresh data. The one exception is a 401: the gateway no longer
recognises the session, so polling stops and the user is sent back to the
login screen with "Your session expired."
