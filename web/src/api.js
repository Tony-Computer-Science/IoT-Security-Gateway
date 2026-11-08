// REST client for docs/api-contract.md v0.3.1.
//
// Rules for this module:
//   - One exported function per endpoint.
//   - Owns the bearer header, JSON parsing, and mapping HTTP status codes to
//     named ApiError objects.
//   - Never touches the DOM.
//   - Keeps the session token in a module-level variable ONLY. It is never
//     written to localStorage, sessionStorage, cookies, or the URL (R2).
//   - Uses relative paths only. No scheme, host, or port.

export const API_VERSION = '0.3.1';

const REQUEST_TIMEOUT_MS = 8000;

// In-memory session token. A page reload intentionally drops the session.
let token = null;

// Error codes the UI can branch on. Server-provided codes (body.error) take
// precedence; these are used when the body is missing or unparseable.
export const ErrorCode = Object.freeze({
  INVALID_CREDENTIALS: 'invalid_credentials',
  LOCKED_OUT: 'locked_out',
  INVALID_SESSION: 'invalid_session',
  INVALID_ACTION: 'invalid_action',
  INVALID_REQUEST: 'invalid_request',
  NOT_PERMITTED: 'not_permitted',
  NOT_FOUND: 'not_found',
  METHOD_NOT_ALLOWED: 'method_not_allowed',
  PAYLOAD_TOO_LARGE: 'payload_too_large',
  RATE_LIMITED: 'rate_limited',
  TARGET_UNREACHABLE: 'target_unreachable',
  COMMAND_UNCONFIRMED: 'command_unconfirmed',
  LINK_DOWN: 'link_down',
  NETWORK: 'network_error', // fetch failed: gateway unreachable
  TIMEOUT: 'timeout', // no response within REQUEST_TIMEOUT_MS
  BAD_RESPONSE: 'bad_response', // response was not the JSON we expected
  HTTP: 'http_error', // any other unexpected status
});

const FALLBACK_BY_STATUS = {
  400: ErrorCode.INVALID_REQUEST,
  401: ErrorCode.INVALID_SESSION,
  403: ErrorCode.NOT_PERMITTED,
  404: ErrorCode.NOT_FOUND,
  405: ErrorCode.METHOD_NOT_ALLOWED,
  413: ErrorCode.PAYLOAD_TOO_LARGE,
  429: ErrorCode.RATE_LIMITED,
  502: ErrorCode.TARGET_UNREACHABLE,
  503: ErrorCode.LINK_DOWN,
  504: ErrorCode.COMMAND_UNCONFIRMED,
};

export class ApiError extends Error {
  /**
   * @param {string} code    one of ErrorCode
   * @param {number} status  HTTP status, or 0 for network/timeout errors
   * @param {object} [body]  parsed error body, if any
   */
  constructor(code, status, body = {}) {
    super(code);
    this.name = 'ApiError';
    this.code = code;
    this.status = status;
    this.retryAfter = typeof body.retry_after === 'number' ? body.retry_after : null;
    this.attemptsRemaining =
      typeof body.attempts_remaining === 'number' ? body.attempts_remaining : null;
  }
}

/** True if a session token is currently held in memory. */
export function hasSession() {
  return token !== null;
}

/** Drop the in-memory token without contacting the server. */
export function clearSession() {
  token = null;
}

async function request(method, path, { body, auth = true } = {}) {
  const headers = { Accept: 'application/json' };
  if (body !== undefined) headers['Content-Type'] = 'application/json';
  if (auth) {
    if (token === null) throw new ApiError(ErrorCode.INVALID_SESSION, 401);
    headers.Authorization = `Bearer ${token}`;
  }

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
  let res;
  try {
    res = await fetch(path, {
      method,
      headers,
      body: body === undefined ? undefined : JSON.stringify(body),
      cache: 'no-store',
      credentials: 'omit',
      signal: controller.signal,
    });
  } catch (err) {
    throw new ApiError(err.name === 'AbortError' ? ErrorCode.TIMEOUT : ErrorCode.NETWORK, 0);
  } finally {
    clearTimeout(timer);
  }

  if (res.status === 204) return null;

  let data = null;
  try {
    data = await res.json();
  } catch {
    // Fall through: handled below.
  }

  if (res.ok) {
    if (data === null || typeof data !== 'object') {
      throw new ApiError(ErrorCode.BAD_RESPONSE, res.status);
    }
    return data;
  }

  const errBody = data && typeof data === 'object' ? data : {};
  const code =
    typeof errBody.error === 'string'
      ? errBody.error
      : FALLBACK_BY_STATUS[res.status] || ErrorCode.HTTP;

  // Any 401 on an authenticated call means the session is gone (expired,
  // logged out elsewhere, or gateway restarted per R2/R7).
  if (auth && res.status === 401) token = null;

  throw new ApiError(code, res.status, errBody);
}

/**
 * POST /api/login. Stores the token in memory on success.
 * @returns {Promise<{expiresIn: number}>}
 */
export async function login(username, password) {
  const data = await request('POST', '/api/login', {
    body: { username, password },
    auth: false,
  });
  if (typeof data.token !== 'string' || typeof data.expires_in !== 'number') {
    throw new ApiError(ErrorCode.BAD_RESPONSE, 200);
  }
  token = data.token;
  return { expiresIn: data.expires_in };
}

/** POST /api/logout. Clears the in-memory token whether or not the call succeeds. */
export async function logout() {
  try {
    await request('POST', '/api/logout');
  } finally {
    token = null;
  }
}

/**
 * GET /api/status
 * @returns {Promise<{output: 'on'|'off', target_reachable: boolean,
 *   link_ok: boolean, uptime_s: number, seq: number}>}
 */
export function getStatus() {
  return request('GET', '/api/status');
}

/**
 * POST /api/command
 * @param {'led_on'|'led_off'} action
 * @returns {Promise<{result: 'ok', output: 'on'|'off'}>}
 */
export function sendCommand(action) {
  return request('POST', '/api/command', { body: { action } });
}

/**
 * GET /api/events
 * @param {{limit?: number, outcome?: 'allowed'|'denied'|'alert'}} [opts]
 * @returns {Promise<{events: Array<{seq: number, uptime_s: number,
 *   type: string, outcome: string, detail: string}>}>}
 */
export function getEvents({ limit, outcome } = {}) {
  const params = new URLSearchParams();
  if (limit !== undefined) params.set('limit', String(limit));
  if (outcome) params.set('outcome', outcome);
  const qs = params.toString();
  return request('GET', qs ? `/api/events?${qs}` : '/api/events');
}
