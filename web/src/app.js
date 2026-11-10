// Application bootstrap: view registry, show/hide switching, and the status
// polling loop, session guard, and the login, control, and events views.
//
// No framework, no router: a view is a <section data-view="name"> toggled
// with the `hidden` attribute.

import * as api from './api.js';
import { downSeconds, timelineSegments } from './timeline.js';

const POLL_INTERVAL_MS = 2000;

// ---------------------------------------------------------------------------
// View registry
// ---------------------------------------------------------------------------

/**
 * @typedef {object} View
 * @property {HTMLElement} el
 * @property {() => void} [onShow]   called after the view becomes visible
 * @property {() => void} [onHide]   called before the view is hidden
 */

/** @type {Record<string, View>} */
const views = {};
let currentView = null;

function registerView(name, hooks = {}) {
  const el = document.querySelector(`[data-view="${name}"]`);
  if (!el) throw new Error(`missing view element: ${name}`);
  views[name] = { el, ...hooks };
}

export function showView(name) {
  if (!views[name]) throw new Error(`unknown view: ${name}`);

  // Session guard: every view except login needs a valid, unexpired token.
  if (name !== 'login' && !hasValidSession()) {
    if (api.hasSession()) {
      expireSession(); // deadline passed before the timer fired
      return;
    }
    if (!isLockedOut()) setLoginMessage('', 'Sign in to continue.');
    name = 'login';
  }

  const next = views[name];
  if (currentView === name) return;

  const prev = currentView && views[currentView];
  if (prev) {
    if (prev.onHide) prev.onHide();
    prev.el.hidden = true;
  }
  next.el.hidden = false;
  currentView = name;

  for (const btn of document.querySelectorAll('[data-nav]')) {
    if (btn.dataset.nav === name) btn.setAttribute('aria-current', 'page');
    else btn.removeAttribute('aria-current');
  }
  if (next.onShow) next.onShow();
}

// ---------------------------------------------------------------------------
// Session lifetime
// ---------------------------------------------------------------------------

// The token itself lives only in api.js, in memory (R2). app.js tracks when
// it expires so the UI can return to login without waiting for a 401.
let sessionExpiresAt = 0; // Date.now() milliseconds; 0 means no session
let sessionTimer = null;

// Checks the deadline as well as the timer, because browsers delay timers in
// background tabs and the timer may fire late.
function hasValidSession() {
  return api.hasSession() && Date.now() < sessionExpiresAt;
}

function startSessionTimer(expiresInS) {
  clearSessionTimer();
  sessionExpiresAt = Date.now() + expiresInS * 1000;
  sessionTimer = setTimeout(expireSession, expiresInS * 1000);
}

function clearSessionTimer() {
  if (sessionTimer !== null) {
    clearTimeout(sessionTimer);
    sessionTimer = null;
  }
  sessionExpiresAt = 0;
}

function expireSession() {
  sessionTimer = null;
  clearSessionTimer();
  api.clearSession();
  setLoginMessage('', 'Your session expired. Sign in again.');
  showView('login');
}

// ---------------------------------------------------------------------------
// Status polling (fixed interval, no WebSocket)
// ---------------------------------------------------------------------------

// Rules:
//   - At most one timer exists at a time (schedulePoll clears before setting).
//   - At most one request is in flight at a time, poll OR command. A tick that
//     finds a poll or a command in flight is skipped, not queued.
//   - The next poll is scheduled only after the current one finishes, so a
//     slow gateway slows polling down instead of stacking requests.
//   - There is one loop for the whole app. Each view that needs refreshing
//     passes its own task to startPolling (status for control, the event log
//     for events), so two views can never refresh at the same time.

let pollTimer = null;
let polling = false;
let pollInFlight = null; // Promise of the current poll, or null
let commandInFlight = false; // set by the control view; pauses polling
let pollTask = null; // async (isCurrent) => void, the active view's refresh
let pollGeneration = 0; // bumped on start/stop so late results are dropped
let rerunWhenFree = false; // an immediate poll was skipped; run it next
let nextTickImmediate = false; // the pending timer was set with delay 0

/**
 * @param {(isCurrent: () => boolean) => Promise<void>} [task]  refresh to
 *   run each tick. It must only apply its result while isCurrent() is true.
 */
export function startPolling(task = pollStatus) {
  if (polling && pollTask === task) return;
  stopPolling();
  polling = true;
  pollTask = task;
  schedulePoll(0);
}

export function stopPolling() {
  polling = false;
  pollTask = null;
  pollGeneration++;
  if (pollTimer !== null) {
    clearTimeout(pollTimer);
    pollTimer = null;
  }
}

function schedulePoll(delayMs) {
  if (pollTimer !== null) clearTimeout(pollTimer);
  nextTickImmediate = delayMs === 0;
  pollTimer = polling ? setTimeout(pollTick, delayMs) : null;
}

async function pollTick() {
  pollTimer = null;
  if (!polling) return;
  if (pollInFlight || commandInFlight) {
    // Skip this tick. If it was meant to run now (view just opened, filter
    // changed), run it as soon as the request in flight finishes.
    if (nextTickImmediate) rerunWhenFree = true;
    schedulePoll(POLL_INTERVAL_MS);
    return;
  }
  if (document.hidden || !api.hasSession()) {
    schedulePoll(POLL_INTERVAL_MS); // skip this tick
    return;
  }
  const gen = pollGeneration;
  rerunWhenFree = false;
  pollInFlight = pollTask(() => polling && gen === pollGeneration);
  await pollInFlight;
  pollInFlight = null;
  schedulePoll(rerunWhenFree ? 0 : POLL_INTERVAL_MS);
}

/** Run the active view's refresh as soon as no request is in flight. */
function pollNow() {
  if (pollInFlight || commandInFlight) rerunWhenFree = true;
  else schedulePoll(0);
}

async function pollStatus(isCurrent) {
  try {
    const status = await api.getStatus();
    if (isCurrent()) renderStatus(status);
  } catch (err) {
    if (isCurrent()) renderStatusError(err);
  }
}

// ---------------------------------------------------------------------------
// Login view
// ---------------------------------------------------------------------------

let lockoutEndsAt = 0; // Date.now() milliseconds; 0 means not locked out

function isLockedOut() {
  return Date.now() < lockoutEndsAt;
}

/**
 * Show a message under the login form using a .state style from style.css.
 * @param {string} state  data-state value ('' for the neutral style)
 * @param {string} text   empty string hides the message
 */
function setLoginMessage(state, text) {
  const msg = document.getElementById('login-message');
  msg.dataset.state = state;
  msg.textContent = text;
  msg.hidden = !text;
}

let loginWired = false;
let lockoutTimer = null;

/**
 * Called each time the login view is shown. Wires the form once.
 * @param {HTMLElement} el  the login <section>
 */
function renderLogin(el) {
  const form = el.querySelector('#login-form');
  if (!loginWired) {
    form.addEventListener('submit', onLoginSubmit);
    loginWired = true;
  }
  if (!isLockedOut()) form.elements.username.focus();
}

async function onLoginSubmit(event) {
  // Handle the submit in JavaScript only. The browser must never send the
  // form itself, which would reload the page and put fields in the URL.
  event.preventDefault();
  const form = event.currentTarget;
  const submit = form.querySelector('button[type="submit"]');
  if (submit.disabled) return; // request in flight, or locked out

  const username = form.elements.username.value;
  const password = form.elements.password.value;
  if (!username || !password) {
    setLoginMessage('denied', 'Enter a username and password.');
    return;
  }

  submit.disabled = true;
  setLoginMessage('pending', 'Signing in…');
  try {
    const { expiresIn } = await api.login(username, password);
    form.reset(); // do not leave the password sitting in the page
    setLoginMessage('', '');
    submit.disabled = false;
    startSessionTimer(expiresIn);
    showView('control');
  } catch (err) {
    form.elements.password.value = '';
    if (err.code === api.ErrorCode.LOCKED_OUT && err.retryAfter !== null) {
      startLockout(err.retryAfter); // keeps the submit button disabled
      return;
    }
    submit.disabled = false;
    setLoginMessage('denied', loginErrorText(err));
    form.elements.password.focus();
  }
}

/** @param {api.ApiError} err */
function loginErrorText(err) {
  switch (err.code) {
    case api.ErrorCode.INVALID_CREDENTIALS: {
      const n = err.attemptsRemaining;
      if (n === null) return 'Wrong username or password.';
      if (n === 0) return 'Wrong username or password. No attempts remaining.';
      return `Wrong username or password. ${n} attempt${n === 1 ? '' : 's'} remaining before lockout.`;
    }
    case api.ErrorCode.LOCKED_OUT:
      return 'Too many failed attempts. Try again later.';
    case api.ErrorCode.NETWORK:
    case api.ErrorCode.TIMEOUT:
      return 'Could not reach the gateway. Check your connection and try again.';
    default:
      return `Sign-in failed (${err.code || 'unknown error'}).`;
  }
}

// Lockout countdown. The end time is stored as a deadline and the display is
// recomputed from it, so a delayed timer never makes the countdown drift.
function startLockout(retryAfterS) {
  stopLockoutTimer();
  lockoutEndsAt = Date.now() + retryAfterS * 1000;
  document.querySelector('#login-form button[type="submit"]').disabled = true;

  // One wrapper element, because .state lays out its children with flex.
  // The seconds span is aria-live="off" so screen readers announce the
  // lockout once instead of every second.
  const text = document.createElement('span');
  const seconds = document.createElement('span');
  seconds.setAttribute('aria-live', 'off');
  seconds.textContent = formatSeconds(retryAfterS);
  text.append('Too many failed attempts. Sign-in is locked. Try again in ', seconds, '.');

  const msg = document.getElementById('login-message');
  msg.dataset.state = 'rate-limited';
  msg.replaceChildren(text);
  msg.hidden = false;

  lockoutTimer = setInterval(() => {
    const left = Math.ceil((lockoutEndsAt - Date.now()) / 1000);
    if (left <= 0) {
      endLockout();
      return;
    }
    const shown = formatSeconds(left);
    if (seconds.textContent !== shown) seconds.textContent = shown;
  }, 250);
}

function endLockout() {
  stopLockoutTimer();
  lockoutEndsAt = 0;
  document.querySelector('#login-form button[type="submit"]').disabled = false;
  setLoginMessage('', 'You can try signing in again.');
}

function stopLockoutTimer() {
  if (lockoutTimer !== null) {
    clearInterval(lockoutTimer);
    lockoutTimer = null;
  }
}

/** 45 -> "45 s", 125 -> "2:05" */
function formatSeconds(s) {
  if (s < 60) return `${s} s`;
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

// ---------------------------------------------------------------------------
// Control view
// ---------------------------------------------------------------------------
//
// NO OPTIMISTIC UPDATES (R6, R7). The displayed LED state changes only when a
// gateway response reports the target's actual state: a 200 from
// POST /api/command, or GET /api/status. Clicking a button never changes it.
// Admission failures leave the target unchanged. A lost reply or missing
// device acknowledgement can leave an unknown physical result. Keep the
// last reported output until another report arrives; never infer success
// or automatically retry from an ambiguous response.

const LINK_DOWN_TEXT =
  'Board-to-board link is down: the station board and the bridge board cannot talk over the SPI/UART wire. Commands are disabled.';
const TARGET_UNREACHABLE_TEXT =
  'Target device unreachable: the station board cannot reach the IoT device on its private Wi-Fi. Commands are disabled.';
const GATEWAY_UNREACHABLE_TEXT =
  'Could not reach the gateway. Check your connection and try again.';

// Last state reported by the gateway. null means "not known yet".
const shown = {
  output: null, target_reachable: null, link_ok: null, uptime_s: null,
  device_uptime_s: null, last_confirmed_command: null, monitoring: null,
};
let liveDeviceMode = false;
let statusFresh = false;
let shownForSession = 0; // sessionExpiresAt of the session `shown` belongs to

let controlWired = false;
let commandCooldownEndsAt = 0; // Date.now() ms; set by a 429 rate_limited
let commandCooldownTimer = null;

/**
 * Called each time the control view is shown. Wires the buttons once.
 * @param {HTMLElement} el  the control <section>
 */
function renderControl(el) {
  if (!controlWired) {
    for (const btn of el.querySelectorAll('[data-action]')) {
      btn.addEventListener('click', () => onCommandClick(btn.dataset.action));
    }
    el.querySelector('#logout-btn').addEventListener('click', onLogoutClick);
    controlWired = true;
  }
  // A new login must not show the previous session's state.
  if (shownForSession !== sessionExpiresAt) {
    shownForSession = sessionExpiresAt;
    statusFresh = false;
    liveDeviceMode = false;
    Object.assign(shown, {
      output: null, target_reachable: null, link_ok: null, uptime_s: null,
      device_uptime_s: null, last_confirmed_command: null, monitoring: null,
    });
    setStateMessage('control-state', '', '');
    setStateMessage('command-result', '', '');
  }
  renderShown();
}

/**
 * A successful status poll.
 * @param {{output: string, target_reachable: boolean, link_ok: boolean,
 *   uptime_s: number, seq: number}} status
 */
function renderStatus(status) {
  const wasHealthy = isHealthy();
  statusFresh = true;
  liveDeviceMode = status.status_source === 'device_http';
  document.getElementById('uptime-label').textContent = liveDeviceMode ? 'API service uptime' : 'Gateway uptime';
  document.getElementById('link-label').textContent = liveDeviceMode ? 'Device channel' : 'Board-to-board link';
  document.getElementById('link-medium').textContent = liveDeviceMode
    ? 'HTTP heartbeat between the API service and target'
    : 'SPI/UART wire, bridge board B to station board A';
  document.getElementById('target-medium').textContent = liveDeviceMode
    ? (status.last_seen_s === null ? 'Waiting for first device report' : `Last report: ${status.last_seen_s}s ago`)
    : 'Private Wi-Fi "IoT-Gateway", station board A to IoT device C';
  const note = document.getElementById('live-test-note');
  note.hidden = !liveDeviceMode;
  const monitoring = status.monitoring && typeof status.monitoring === 'object' ? status.monitoring : null;
  shown.monitoring = liveDeviceMode ? monitoring : null;
  note.textContent = monitoring && monitoring.enabled === true
    ? 'Live device test: the gateway is checked from this computer (Board A by ping, the path through the SPI/UART link and Board B by an outside connection). Network isolation is not measured. LED state is the last device report.'
    : 'Live device test: gateway A/B link and network isolation are not measured. LED state is the last device report.';
  document.querySelector('.path').dataset.live = String(liveDeviceMode);
  document.querySelector('.path').setAttribute('aria-label', liveDeviceMode ? 'Device test status' : 'Signal path from the gateway to the device');
  const nodes = document.querySelectorAll('.node-box');
  nodes[0].textContent = liveDeviceMode ? 'API' : 'B';
  nodes[1].textContent = liveDeviceMode ? 'Wi-Fi' : 'A';
  if (status.output === 'on' || status.output === 'off') shown.output = status.output;
  shown.target_reachable = status.target_reachable === true;
  shown.link_ok = status.link_ok === true;
  shown.uptime_s = typeof status.uptime_s === 'number' ? status.uptime_s : null;
  shown.device_uptime_s = Number.isSafeInteger(status.device_uptime_s) && status.device_uptime_s >= 0
    ? status.device_uptime_s : null;
  const confirmed = status.last_confirmed_command;
  shown.last_confirmed_command = confirmed &&
    ((confirmed.action === 'led_on' && confirmed.output === 'on') ||
     (confirmed.action === 'led_off' && confirmed.output === 'off')) &&
    Number.isSafeInteger(confirmed.service_uptime_s) && confirmed.service_uptime_s >= 0 &&
    confirmed.service_uptime_s <= shown.uptime_s ? confirmed : null;

  // On the transition from healthy into a problem, clear the last command
  // result: a green "Done" under a live failure would read as "all fine"
  // (R6, R7). Only on the transition, so during normal operation a success
  // stays visible. A command that failed with 502/503 has already marked
  // `shown` unhealthy, so its more specific failure message is kept.
  if (wasHealthy && !isHealthy()) setStateMessage('command-result', '', '');

  // Link down is checked first, so if both are reported only link down is
  // shown. (The old reason, that A could not see the target without B, no
  // longer holds: C is on A's own Wi-Fi. The order itself is unchanged.)
  if (!shown.link_ok) {
    setStateMessage('control-state', 'link-down', liveDeviceMode ? 'Device heartbeat unavailable. Commands are disabled; the LED display is the last reported state.' : LINK_DOWN_TEXT);
  } else if (!shown.target_reachable) {
    setStateMessage('control-state', 'target-unreachable', TARGET_UNREACHABLE_TEXT);
  } else {
    setStateMessage('control-state', '', '');
    // A link or target failure from an earlier command has now cleared.
    const last = document.getElementById('command-result').dataset.state;
    if (last === 'link-down' || last === 'target-unreachable') {
      setStateMessage('command-result', '', '');
    }
  }
  renderShown();
}

/**
 * A failed status poll. The last known state stays on screen; only the
 * message changes, so a brief network glitch never blanks the view.
 * @param {api.ApiError} err
 */
function renderStatusError(err) {
  if (err.code === api.ErrorCode.INVALID_SESSION) {
    sessionLost();
    return;
  }
  const text =
    err.code === api.ErrorCode.NETWORK || err.code === api.ErrorCode.TIMEOUT
      ? GATEWAY_UNREACHABLE_TEXT
      : `Status update failed (${err.code}).`;
  statusFresh = false;
  if (liveDeviceMode) {
    document.getElementById('target-medium').textContent = 'Report age unavailable · status updates stopped';
  }
  setStateMessage('control-state', 'denied', `${text} Showing the last known state. Commands are disabled until status recovers.`);
  renderShown();
}

async function onCommandClick(action) {
  if (commandInFlight || !commandsAllowed()) return;
  if (!hasValidSession()) {
    sessionLost();
    return;
  }

  // Pause polling. pollTick skips while this flag is set.
  commandInFlight = true;
  renderShown(); // disables both buttons
  // Pending shows what was asked for, never the result. The LED output row
  // keeps showing the last confirmed state until the gateway answers.
  setStateMessage('command-result', 'pending',
    `Sending "${action === 'led_on' ? 'turn on' : 'turn off'}" to the device…`);

  try {
    // Never overlap with a poll: if one is in flight, wait for it to finish.
    if (pollInFlight) await pollInFlight;
    if (!hasValidSession() || !api.hasSession()) {
      sessionLost();
      return;
    }
    if (!commandsAllowed()) {
      // The poll we just waited for reported a problem. Do not send.
      setStateMessage('command-result', '', 'Command not sent. See the gateway status above.');
      return;
    }

    const res = await api.sendCommand(action);
    if (res.output !== 'on' && res.output !== 'off') {
      throw new api.ApiError(api.ErrorCode.BAD_RESPONSE, 200);
    }
    // The only place a command changes the displayed output: a 200 whose body
    // carries the state reported back from the target.
    shown.output = res.output;
    setStateMessage('command-result', 'ok', `Done. The LED is now ${res.output}.`);
  } catch (err) {
    handleCommandError(err); // never touches shown.output
  } finally {
    commandInFlight = false;
    renderShown();
    schedulePoll(POLL_INTERVAL_MS); // resume polling
  }
}

/** @param {api.ApiError} err */
function handleCommandError(err) {
  const unchanged = 'The LED was not changed.';
  switch (err.code) {
    case api.ErrorCode.INVALID_ACTION:
      setStateMessage('command-result', 'denied', `That command is not recognised. ${unchanged}`);
      break;
    case api.ErrorCode.NOT_PERMITTED:
      setStateMessage('command-result', 'denied', `The gateway does not permit that command. ${unchanged}`);
      break;
    case api.ErrorCode.INVALID_SESSION:
      sessionLost();
      break;
    case api.ErrorCode.RATE_LIMITED:
      if (err.retryAfter !== null) startCommandCooldown(err.retryAfter);
      else setStateMessage('command-result', 'rate-limited', `Too many commands. Wait and try again. ${unchanged}`);
      break;
    case api.ErrorCode.TARGET_UNREACHABLE:
      // The response says the target is unreachable right now. Disable the
      // buttons until a poll says otherwise. The output row is not touched.
      shown.target_reachable = false;
      setStateMessage('control-state', 'target-unreachable', TARGET_UNREACHABLE_TEXT);
      setStateMessage('command-result', 'target-unreachable',
        `Command not delivered: the API service could not reach the IoT device. ${unchanged}`);
      break;
    case api.ErrorCode.LINK_DOWN:
      shown.link_ok = false;
      setStateMessage('control-state', 'link-down', liveDeviceMode ? 'Device heartbeat unavailable. Commands are disabled; the LED display is the last reported state.' : LINK_DOWN_TEXT);
      setStateMessage('command-result', 'link-down',
        `Command not delivered: the station board and the bridge board cannot talk over the SPI/UART wire. ${unchanged}`);
      break;
    case api.ErrorCode.COMMAND_FAILED:
      // The device answered and said the switch did not take. That is not
      // optical proof the LED is untouched, so name the device's own report
      // instead of saying "unchanged". The next poll refreshes the display.
      setStateMessage('command-result', 'denied',
        `The device reported that switching the LED failed${err.reportedOutput ? ` and says the LED is ${err.reportedOutput}` : ''}. The command will not be retried automatically.`);
      break;
    case api.ErrorCode.SERVER_BUSY:
      setStateMessage('command-result', 'rate-limited',
        `The API service is busy and did not accept the command. Wait a moment and try again. ${unchanged}`);
      break;
    case api.ErrorCode.COMMAND_UNCONFIRMED:
      setStateMessage('command-result', 'denied',
        'The device did not confirm this command. Its LED may have changed. Waiting for a fresh device report; the command will not be retried automatically.');
      break;
    case api.ErrorCode.NETWORK:
    case api.ErrorCode.TIMEOUT:
      // The command may or may not have reached the gateway, so do not claim
      // the LED was unchanged. The next poll shows the real state.
      setStateMessage('command-result', 'denied',
        `${GATEWAY_UNREACHABLE_TEXT} The result is unknown; the next status update will show the real state.`);
      break;
    default:
      setStateMessage('command-result', 'denied',
        `Command failed (${err.code}). The LED state shown is the last one the gateway confirmed.`);
  }
}

// Rate-limit cooldown: same deadline-based countdown as the login lockout.
function startCommandCooldown(retryAfterS) {
  stopCommandCooldownTimer();
  commandCooldownEndsAt = Date.now() + retryAfterS * 1000;

  const text = document.createElement('span');
  const seconds = document.createElement('span');
  seconds.setAttribute('aria-live', 'off'); // announce once, not every second
  seconds.textContent = formatSeconds(retryAfterS);
  text.append('Too many commands. The LED was not changed. Try again in ', seconds, '.');
  const msg = document.getElementById('command-result');
  msg.dataset.state = 'rate-limited';
  msg.replaceChildren(text);
  msg.hidden = false;

  commandCooldownTimer = setInterval(() => {
    const left = Math.ceil((commandCooldownEndsAt - Date.now()) / 1000);
    if (left <= 0) {
      stopCommandCooldownTimer();
      commandCooldownEndsAt = 0;
      // Do not announce "send commands again" while the link or target is down.
      setStateMessage('command-result', '', commandsAllowed() ? 'You can send commands again.' : '');
      renderShown();
      return;
    }
    const s = formatSeconds(left);
    if (seconds.textContent !== s) seconds.textContent = s;
  }, 250);
  renderShown();
}

function stopCommandCooldownTimer() {
  if (commandCooldownTimer !== null) {
    clearInterval(commandCooldownTimer);
    commandCooldownTimer = null;
  }
}

function isHealthy() {
  return shown.link_ok === true && shown.target_reachable === true;
}

// R7: commands are enabled only when the last status said both the link and
// the target are up, and nothing else is blocking them.
function commandsAllowed() {
  return statusFresh && isHealthy() && Date.now() >= commandCooldownEndsAt;
}

function renderShown() {
  const label = (v, yes, no) => (v === null ? 'Unknown' : v ? yes : no);
  document.getElementById('status-output').textContent =
    shown.output === null ? 'Unknown' : shown.output === 'on' ? 'On' : 'Off';
  document.getElementById('status-target').textContent = label(shown.target_reachable, 'Reachable', 'Unreachable');
  document.getElementById('status-link').textContent = label(shown.link_ok, 'Up', 'Down');
  document.getElementById('status-uptime').textContent =
    shown.uptime_s === null ? 'Unknown' : formatUptime(shown.uptime_s);

  document.getElementById('live-monitoring').hidden = !liveDeviceMode;
  document.getElementById('device-uptime').textContent = shown.device_uptime_s === null
    ? 'Unknown' : formatUptime(shown.device_uptime_s);
  document.getElementById('device-uptime-note').textContent = shown.device_uptime_s === null
    ? 'Waiting for first device report'
    : !statusFresh ? 'Last reported uptime · status updates unavailable'
      : !isHealthy() ? 'Last reported uptime · device offline'
        : 'Since ESP restart · measured at last report';
  const confirmed = shown.last_confirmed_command;
  document.getElementById('last-command').textContent = confirmed
    ? `${confirmed.action === 'led_on' ? 'Turn on' : 'Turn off'} → ${confirmed.output.toUpperCase()} confirmed`
    : 'None yet';
  document.getElementById('last-command-note').textContent = confirmed
    ? `Confirmed at service uptime ${formatUptime(confirmed.service_uptime_s)}${statusFresh ? '' : ' · history may be stale'}`
    : 'No command confirmed since service start';

  // Presentation hooks for CSS (the LED lamp and the signal-path wires).
  // CONFIRMED STATE ONLY: these read `shown`, which is written only from
  // gateway responses, never from the action the user requested. This
  // function also runs when a click starts a command (to disable the
  // buttons); at that moment `shown` still holds the last confirmed state,
  // so the lamp does not change. Setting data-value from the requested
  // action would be an optimistic update and would break R7.
  document.getElementById('status-output').dataset.value = shown.output ?? 'unknown';
  document.getElementById('status-target').dataset.value = label(shown.target_reachable, 'reachable', 'unreachable').toLowerCase();
  document.getElementById('status-link').dataset.value = label(shown.link_ok, 'up', 'down').toLowerCase();

  const disabled = commandInFlight || !commandsAllowed();
  for (const btn of document.querySelectorAll('#view-control [data-action]')) {
    btn.disabled = disabled;
  }
  renderGatewayMonitoring();
}

// ---------------------------------------------------------------------------
// Live gateway monitoring (Task 33). Every value comes from the service's
// own checks (`status.monitoring`); nothing here is inferred in the browser.
// ---------------------------------------------------------------------------

const GW_DIAGNOSIS = {
  forwarding: ['ok', 'Gateway forwarding: Board A answers, and traffic passes through the SPI/UART link and Board B.'],
  board_a_down: ['link-down', 'Board A is not answering. Check its power, and that this computer is on its Wi-Fi.'],
  link_or_upstream_down: ['denied', "Board A answers, but no traffic passes through it: the SPI/UART link or Board B's upstream Wi-Fi is down. These checks cannot tell which."],
  checking: ['pending', 'Checking the gateway…'],
  not_monitored: ['', 'Gateway checks are off for this service, so only the IoT device is monitored.'],
};
const GW_STATE_TEXT = { up: 'Up', down: 'Down', unknown: 'Checking', off: 'Not monitored' };

function renderGatewayMonitoring() {
  const m = shown.monitoring;
  const panel = document.getElementById('gateway-monitoring');
  panel.hidden = !liveDeviceMode || !m;
  if (panel.hidden) return;
  const windowS = Number.isFinite(m.window_s) && m.window_s > 0 ? m.window_s : 300;
  const nowS = Number.isFinite(m.now_s) ? m.now_s : 0;
  const stale = statusFresh ? '' : ' Status updates stopped; this may be out of date.';
  const [state, text] = GW_DIAGNOSIS[m.diagnosis] || ['', 'Gateway state unknown.'];
  setStateMessage('gw-diagnosis', statusFresh ? state : '', text + stale);
  document.getElementById('gw-axis-start').textContent = `${Math.round(windowS / 60)} min ago`;
  document.getElementById('gw-note').textContent = m.enabled === true
    ? 'The path check needs this computer to route through Board A only. Board A and the path are checked from this computer; the device line is its heartbeat.'
    : 'Start the service without --no-gateway-checks, on Board A\'s Wi-Fi, to check Boards A and B.';

  for (const key of ['board_a', 'gateway_path', 'device']) {
    const c = m[key] && typeof m[key] === 'object' ? m[key] : { state: 'unknown', history: [] };
    const value = GW_STATE_TEXT[c.state] ? c.state : 'unknown';
    const el = document.getElementById(`gw-${key}-state`);
    el.textContent = GW_STATE_TEXT[value];
    el.dataset.value = value;
    if (key !== 'device' && typeof c.target === 'string') {
      document.getElementById(`gw-${key}-medium`).textContent =
        key === 'board_a' ? `Ping to ${c.target} from this computer` : `Connection to ${c.target} through A, the SPI/UART link and B`;
    }
    const parts = [];
    if (Number.isFinite(c.latency_ms)) parts.push(`${c.latency_ms} ms`);
    if (Number.isFinite(c.checked_s_ago)) parts.push(`checked ${c.checked_s_ago}s ago`);
    const down = downSeconds(c.history, nowS, windowS);
    if (down > 0) parts.push(`down ${Math.round(down)}s in the last ${Math.round(windowS / 60)} min`);
    document.getElementById(`gw-${key}-detail`).textContent = parts.join(' · ');
    renderStrip(document.getElementById(`gw-${key}-strip`), c.history, nowS, windowS);
  }
}

function renderStrip(strip, history, nowS, windowS) {
  const segments = timelineSegments(history, nowS, windowS);
  strip.replaceChildren(...segments.map((s) => {
    const span = document.createElement('span');
    span.className = `gw-seg gw-seg-${s.state}`;
    // CSSOM assignments are permitted by the page's style-src 'self' CSP.
    span.style.left = `${s.startPct}%`;
    span.style.width = `${s.widthPct}%`;
    return span;
  }));
  const downs = segments.filter((s) => s.state === 'down').length;
  strip.setAttribute('aria-label', segments.length === 0 ? 'No history yet'
    : downs === 0 ? 'No outages in the window' : `${downs} outage${downs === 1 ? '' : 's'} in the window`);
}

/** Set a .state message; skips identical updates so screen readers are not spammed. */
function setStateMessage(id, state, text) {
  const msg = document.getElementById(id);
  if (msg.dataset.state === state && msg.textContent === text && msg.hidden === !text) return;
  msg.dataset.state = state;
  msg.textContent = text;
  msg.hidden = !text;
}

// 401 from a poll or a command: the gateway no longer knows this session.
function sessionLost() {
  stopPolling();
  expireSession(); // shows "Your session expired. Sign in again."
}

async function onLogoutClick() {
  stopPolling();
  stopCommandCooldownTimer();
  commandCooldownEndsAt = 0;
  try {
    await api.logout(); // clears the in-memory token even if the call fails
  } catch {
    // The token is gone locally either way.
  }
  clearSessionTimer();
  setLoginMessage('', 'You have signed out.');
  showView('login');
}

/** 3725 -> "1:02:05" */
function formatUptime(s) {
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return `${h}:${String(m).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
}

// ---------------------------------------------------------------------------
// Events view
// ---------------------------------------------------------------------------
//
// Shows the gateway's bounded event log (R5), newest first. This view has no
// timer of its own: while it is open, the shared poll loop runs pollEvents
// instead of the status poll, so an events refresh and a status poll can
// never be in flight at the same time.

const EVENT_TYPE_LABELS = { auth: 'Auth', command: 'Command', gateway: 'Gateway', rate_limit: 'Rate limit', target: 'Target' };

let eventsWired = false;
let eventsLoaded = false; // a list has been shown for this session
let eventsKey = null; // filter + seqs currently on screen; skips no-op redraws
let eventsForSession = 0; // sessionExpiresAt of the session the list belongs to

/**
 * Called each time the events view is shown. Wires the controls once and
 * hands the shared poll loop the events refresh.
 * @param {HTMLElement} el  the events <section>
 */
function renderEvents(el) {
  if (!eventsWired) {
    el.querySelector('#events-outcome').addEventListener('change', () => {
      eventsKey = null;
      setStateMessage('events-state', 'pending', 'Loading events…');
      pollNow();
    });
    el.querySelector('#events-refresh').addEventListener('click', pollNow);
    eventsWired = true;
  }
  // A new login must not show the previous session's log.
  if (eventsForSession !== sessionExpiresAt) {
    eventsForSession = sessionExpiresAt;
    eventsLoaded = false;
    eventsKey = null;
    document.getElementById('events-body').replaceChildren();
    document.getElementById('events-table-wrap').hidden = true;
    document.getElementById('events-empty').hidden = true;
  }
  if (!eventsLoaded) setStateMessage('events-state', 'pending', 'Loading events…');
  startPolling(pollEvents);
}

// Filtering is done by the gateway (?outcome=), not in the browser, so the
// list is always what the gateway actually holds for that outcome.
async function pollEvents(isCurrent) {
  const outcome = document.getElementById('events-outcome').value;
  try {
    const data = await api.getEvents(outcome ? { outcome } : {});
    // The filter changed while this request was in flight: drop the result.
    // The change handler already asked for a fresh poll.
    if (!isCurrent() || outcome !== document.getElementById('events-outcome').value) return;
    if (!Array.isArray(data.events)) throw new api.ApiError(api.ErrorCode.BAD_RESPONSE, 200);
    showEvents(data.events, outcome);
  } catch (err) {
    if (isCurrent()) renderEventsError(err);
  }
}

/**
 * A failed refresh. Like a failed status poll, the last loaded list stays on
 * screen and the error is shown beside it.
 * @param {api.ApiError} err
 */
function renderEventsError(err) {
  if (err.code === api.ErrorCode.INVALID_SESSION) {
    sessionLost();
    return;
  }
  const text =
    err.code === api.ErrorCode.NETWORK || err.code === api.ErrorCode.TIMEOUT
      ? GATEWAY_UNREACHABLE_TEXT
      : `Event log refresh failed (${err.code}).`;
  const after = eventsLoaded ? 'Showing the last loaded list.' : 'No events loaded yet.';
  setStateMessage('events-state', 'denied', `${text} ${after}`);
}

function showEvents(events, outcome) {
  setStateMessage('events-state', '', '');
  eventsLoaded = true;

  const list = events.slice().sort((a, b) => b.seq - a.seq); // newest first
  const key = `${outcome}:${list.map((e) => e.seq).join(',')}`;
  if (key === eventsKey) return; // nothing new: leave the DOM alone
  eventsKey = key;

  const empty = document.getElementById('events-empty');
  const wrap = document.getElementById('events-table-wrap');
  const tbody = document.getElementById('events-body');

  if (list.length === 0) {
    empty.textContent = outcome
      ? `No ${outcome} events have been recorded yet.`
      : 'No events have been recorded yet.';
    empty.hidden = false;
    wrap.hidden = true;
    tbody.replaceChildren();
    return;
  }

  const anchor = captureScrollAnchor(tbody);
  tbody.replaceChildren(...list.map(eventRow));
  empty.hidden = true;
  wrap.hidden = false;
  restoreScrollAnchor(tbody, anchor);
}

// All server text goes in with textContent, never innerHTML: `detail` comes
// from the gateway and must not be able to inject markup.
function eventRow(ev) {
  const tr = document.createElement('tr');
  tr.dataset.seq = String(ev.seq);
  const cell = (text) => {
    const td = document.createElement('td');
    td.textContent = text;
    return td;
  };
  const badge = document.createElement('span');
  badge.className = 'state';
  badge.dataset.state = eventState(ev);
  badge.textContent = ev.outcome;
  const outcomeCell = document.createElement('td');
  outcomeCell.append(badge);

  tr.append(
    cell(String(ev.seq)),
    cell(typeof ev.uptime_s === 'number' ? formatUptime(ev.uptime_s) : '?'),
    cell(EVENT_TYPE_LABELS[ev.type] || String(ev.type)),
    outcomeCell,
    cell(String(ev.detail)),
  );
  return tr;
}

// Outcome badges reuse the control view's state styles. An alert takes the
// style of whatever raised it, so a rate-limit alert looks like the
// rate-limited state and a target alert looks like target-unreachable.
function eventState(ev) {
  if (ev.outcome === 'allowed') return 'ok';
  if (ev.outcome === 'alert' && ev.type === 'rate_limit') return 'rate-limited';
  if (ev.outcome === 'alert' && ev.type === 'target') return 'target-unreachable';
  return 'denied';
}

// Scroll preservation. Before redrawing, note which row is at the top of the
// screen and where; afterwards, scroll so that same row is in the same place.
// New events then appear above the reader without moving what they are
// reading. At the very top of the page nothing is anchored, so new events
// stay visible as they arrive.
function captureScrollAnchor(tbody) {
  if (window.scrollY === 0) return null;
  for (const row of tbody.rows) {
    const r = row.getBoundingClientRect();
    if (r.bottom > 0) return { seq: row.dataset.seq, top: r.top };
  }
  return null;
}

function restoreScrollAnchor(tbody, anchor) {
  if (!anchor) return;
  const row = tbody.querySelector(`tr[data-seq="${anchor.seq}"]`);
  if (row) window.scrollBy(0, row.getBoundingClientRect().top - anchor.top);
}

// ---------------------------------------------------------------------------
// Theme toggle (presentation only)
// ---------------------------------------------------------------------------

// Light is the default on every page load: the demo is projected and light
// was tested for that, so the demo must not depend on the laptop's theme.
// Dark is a deliberate manual choice. It is never saved (no storage, no
// cookie), so a reload always returns to light.
function initThemeToggle() {
  const btn = document.getElementById('theme-toggle');
  btn.addEventListener('click', () => {
    const dark = btn.getAttribute('aria-pressed') !== 'true';
    btn.setAttribute('aria-pressed', String(dark));
    document.documentElement.dataset.theme = dark ? 'dark' : 'light';
  });
}

// ---------------------------------------------------------------------------
// Bootstrap
// ---------------------------------------------------------------------------

function init() {
  registerView('login', {
    onShow: () => renderLogin(views.login.el),
  });
  registerView('control', {
    onShow: () => {
      renderControl(views.control.el);
      startPolling();
    },
    onHide: stopPolling,
  });
  registerView('events', {
    onShow: () => renderEvents(views.events.el),
    onHide: stopPolling,
  });

  for (const btn of document.querySelectorAll('[data-nav]')) {
    btn.addEventListener('click', () => showView(btn.dataset.nav));
  }
  initThemeToggle();

  // Poll right away when the tab becomes visible again instead of waiting out
  // the remaining interval. pollTick skips if a request is already in flight,
  // so this can never start a second, overlapping poll chain.
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden && polling) schedulePoll(0);
  });

  showView('login');
}

// Module scripts are deferred, so the DOM is parsed by the time this runs.
init();
