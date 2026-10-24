// Application bootstrap: view registry, show/hide switching, and the status
// polling loop, session guard, and the login view. Control and events
// rendering are still TODO stubs.
//
// No framework, no router: a view is a <section data-view="name"> toggled
// with the `hidden` attribute.

import * as api from './api.js';

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

let pollTimer = null;
let polling = false;

export function startPolling() {
  if (polling) return;
  polling = true;
  pollOnce();
}

export function stopPolling() {
  polling = false;
  if (pollTimer !== null) {
    clearTimeout(pollTimer);
    pollTimer = null;
  }
}

// setTimeout chain rather than setInterval, so a slow response (e.g. injected
// latency) never causes overlapping requests.
async function pollOnce() {
  pollTimer = null;
  if (!polling) return;
  if (api.hasSession() && !document.hidden) {
    try {
      renderStatus(await api.getStatus());
    } catch (err) {
      renderStatusError(err);
    }
  }
  if (polling) pollTimer = setTimeout(pollOnce, POLL_INTERVAL_MS);
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
// View rendering: TODO stubs. Signatures are fixed; fill in the bodies.
// ---------------------------------------------------------------------------

/**
 * TODO(control view): fill #status-* fields; set #control-state to
 * link-down / target-unreachable when appropriate; disable command buttons
 * when !status.link_ok (R7).
 * @param {{output: string, target_reachable: boolean, link_ok: boolean,
 *   uptime_s: number, seq: number}} status
 */
function renderStatus(status) {}

/**
 * TODO(control view): map err.code to a .state style. invalid_session ->
 * stopPolling() and showView('login').
 * @param {api.ApiError} err
 */
function renderStatusError(err) {}

/**
 * TODO(control view): wire [data-action] buttons -> api.sendCommand();
 * show 'pending' while in flight, then ok / denied / rate-limited /
 * target-unreachable / link-down.
 * @param {HTMLElement} el  the control <section>
 */
function renderControl(el) {}

/**
 * TODO(events view): fetch with api.getEvents({outcome}) and render rows
 * into #events-body using textContent only (never innerHTML: `detail` is
 * server-provided text).
 * @param {HTMLElement} el  the events <section>
 */
function renderEvents(el) {}

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
  });

  for (const btn of document.querySelectorAll('[data-nav]')) {
    btn.addEventListener('click', () => showView(btn.dataset.nav));
  }

  // Poll immediately when the tab becomes visible again instead of waiting
  // out the remaining interval.
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden && polling) {
      stopPolling();
      startPolling();
    }
  });

  showView('login');
}

// Module scripts are deferred, so the DOM is parsed by the time this runs.
init();
