# Web control view: how it works

Code: the "Control view" and "Status polling" sections of `web/src/app.js`.
Contract: `GET /api/status` and `POST /api/command` in `docs/api-contract.md`.

The website/API runs as a separate service outside the boards. The demo
hosts it on the local computer at 192.168.78.10:8000. Board C polls the
service and acknowledges commands; local control does not traverse the
station/bridge link. See [Setting Up](../../README.md#setting-up).

## What the user sees

Three status rows are always visible: the LED output (On or Off), whether the
target device is reachable, and whether its channel is available. Below
them are two buttons, "Turn on" and "Turn off", and two message lines. The
first message line shows the gateway's health. The second shows what happened
to the user's last click.

## Why there are no optimistic updates

Many apps change the screen the moment you click, then fix it if the request
fails. This view does not. The LED row changes only when the gateway answers
with a device-reported state: a successful command response, a status
update, or the reported output on an acknowledged `command_failed` response. While a command is on its way, the view shows a "Sending" message and
the LED row keeps its old value.

For example, if the user clicks "Turn on" while Board C loses Wi-Fi, an
immediate screen change could look successful even though the device never
received the command. Conversely, a lost acknowledgement can mean the LED
changed without the service confirming it. The service returns 504 for an
unconfirmed delivered command; the UI keeps the last confirmed output,
states that the result is unknown, and reconciles a later device report.
It never retries automatically or claims an ambiguous result left the LED
unchanged.

## Link down versus target unreachable

The real service derives both `link_ok` and `target_reachable` from a valid
device heartbeat within eight seconds. They measure the device channel,
not SPI/UART or internet access. An expired heartbeat disables commands and
shows the channel-unavailable style (striped box, ⨯ symbol). A command that
was never delivered returns 502 and uses the target-unreachable style
(dotted box, ⚠ symbol) until status is refreshed.

Only the standalone mock simulates independent A/B-link and target failures.
Its injected 503 must not be mistaken for hardware link monitoring by the
real PC service.

## How the polling loop avoids overlap

The page asks for status every 2 seconds. It never has two requests open at
once:

1. Only one timer exists at a time. It refreshes the active Control or Events
   view; switching views discards late results from the old view. Setting a new
   timer cancels the old one.
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

## Live monitoring

The real service identifies itself with `status_source: "device_http"`. Before
the first report, output is Unknown. The view shows ESP uptime at the latest
report separately from API service uptime and last confirmed command history.
Failed status refreshes retain these values with a stale label and disable
commands until fresh status returns. Device restarts reset ESP uptime; history
survives device restarts and clears when the service restarts.

An exact negative device acknowledgement is 502 `command_failed`, distinct
from a missing acknowledgement (504). The UI can show the accompanying reported
output without claiming optical confirmation. A 503 `server_busy` means the
service rejected the browser request for capacity; it is not an A/B link failure.
