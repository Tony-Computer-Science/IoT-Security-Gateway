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
  const next = views[name];
  if (!next) throw new Error(`unknown view: ${name}`);
  if (currentView === name) return;

  // TODO(view guard): once login is implemented, redirect to 'login' when
  // !api.hasSession() and name !== 'login'.

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
// View rendering: TODO stubs. Signatures are fixed; fill in the bodies.
// ---------------------------------------------------------------------------

/**
 * TODO(login view): wire #login-form submit -> api.login(); show
 * invalid_credentials (with attempts remaining) and locked_out (with
 * retry_after) using the .state styles; on success showView('control').
 * @param {HTMLElement} el  the login <section>
 */
function renderLogin(el) {}

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
