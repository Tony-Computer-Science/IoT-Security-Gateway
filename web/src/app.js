// Application bootstrap: view registry, show/hide switching, and the status
// polling loop. View rendering is intentionally left as TODO stubs.
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

/**
 * TODO(login view): wire #login-form submit -> api.login(); show
 * invalid_credentials (with attempts remaining) and locked_out (with
 * retry_after) using the .state styles; on success showView('control').
 * @param {HTMLElement} el  the login <section>
 */
function renderLogin(el) {}

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
