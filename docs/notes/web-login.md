# Web login view: how it works

Code: `web/src/app.js` (login view, session guard) and `web/src/api.js`
(the HTTP call and the token). Contract: `POST /api/login` in
`docs/api-contract.md`.

The login endpoint belongs to the separate web service, hosted on a local
computer for the demo, not to an ESP32 board. See
[Setting Up](../../README.md#setting-up). The demo uses plain HTTP on the
private lab network; it does not implement the original R2 encrypted-client
connection requirement.

## The login flow, step by step

1. The page always starts on the login view. There is no saved session to
   restore, so a fresh page load is always signed out.
2. When the user clicks "Sign in" or presses Enter, the form's submit event
   fires. The handler calls `preventDefault()` first, so the browser never
   sends the form itself. Without that, the page would reload and the
   username and password could end up in the URL.
3. If either field is empty, the view shows an error and does not contact
   the gateway.
4. The submit button is disabled and a "Signing in" message appears using
   the `pending` state style. Disabling the button stops double submits.
5. `api.login()` sends the username and password to the service's
   `POST /api/login` endpoint at the same origin as the page.
6. **Success (200):** `api.js` keeps the token in a variable. The form is
   cleared so the password is not left in the page. A timer is started from
   `expires_in`, and the view switches to Control.
7. **Wrong password (401):** the `denied` style shows "Wrong username or
   password" and how many attempts remain before lockout, taken from
   `attempts_remaining`. The password field is cleared.
8. **Locked out (429):** see the countdown section below.

## The session guard

Every time the app switches views, `showView()` checks for a valid session.
That means a token exists and its expiry time has not passed. If not, the
user is sent back to the login view. When the expiry timer fires, the app
drops the token and shows "Your session expired." The guard also checks the
clock directly, because browsers can delay timers in background tabs.

## Why the token is not saved anywhere

Requirement R2 says sessions expire, are invalidated on restart, and tokens
never appear in logs. The project's rule, based on R2, is that the browser
keeps the token in memory only. It lives in a JavaScript variable inside
`api.js` and is never written to localStorage, sessionStorage, cookies, or
the URL.

- Browser storage can be read by any script running on the page, and it
  stays on disk after the tab closes. A variable disappears when the page
  closes.
- Reloading the page signs the user out. This is on purpose: it matches the
  service, which forgets all sessions when it restarts.
- The build script (`web/scripts/build.py`) fails if the code mentions
  localStorage, sessionStorage, or document.cookie, so this cannot slip in
  by accident.

## How the lockout countdown works

After too many wrong passwords, the gateway answers 429 with `retry_after`,
the number of seconds until login is allowed again (R4).

1. The app works out the exact time the lockout ends: now plus
   `retry_after` seconds. This is the deadline.
2. The message switches to the `rate-limited` style and the submit button is
   disabled, so the user cannot keep trying during the lockout.
3. Four times a second, a timer works out how many whole seconds are left
   until the deadline and updates the number on screen (for example "1:00",
   then "57 s").
4. Because the time left is always worked out from the deadline, the
   countdown stays accurate even if the browser runs the timer late.
5. When the time left reaches zero, the timer stops, the button is enabled
   again, and the message says the user can try again.

For screen reader users, the lockout is announced once. The changing number
is marked so it is not read aloud every second.

The countdown only controls the button. The real lockout is enforced by the
gateway: if someone bypasses the page and calls the API directly, they still
get 429 until the lockout ends.
