// Behavior tests for web/src/api.js, the browser's REST client.
// Run: node --test tools/test/test_web_api.mjs   (Node 18+, no dependencies)
//
// The UI branches on the ErrorCode values this module produces, so these
// tests pin the status/body -> error mapping, token handling and transport
// failures. They stub fetch; they do not exercise the DOM in app.js.
import assert from 'node:assert/strict';
import { beforeEach, test } from 'node:test';

import * as api from '../../web/src/api.js';

const calls = [];
let responses = [];

function respond(status, body) {
  responses.push(() => Promise.resolve(new Response(
    body === undefined ? null : JSON.stringify(body),
    { status, headers: body === undefined ? {} : { 'Content-Type': 'application/json' } },
  )));
}

globalThis.fetch = (path, init) => {
  calls.push({ path, init });
  const next = responses.shift();
  if (!next) throw new Error(`unexpected request ${init.method} ${path}`);
  return next();
};

async function signIn() {
  respond(200, { token: 'session-token', expires_in: 900 });
  await api.login('operator', 'pw');
}

async function rejection(promise) {
  try {
    await promise;
  } catch (err) {
    return err;
  }
  assert.fail('expected the request to fail');
}

beforeEach(() => {
  calls.length = 0;
  responses = [];
  api.clearSession();
});

test('login keeps the token in memory and sends it only as a bearer header', async () => {
  assert.equal(api.hasSession(), false);
  await signIn();
  assert.equal(api.hasSession(), true);
  assert.equal(calls[0].init.headers.Authorization, undefined);
  assert.equal(calls[0].init.credentials, 'omit');

  respond(200, { result: 'ok', output: 'on' });
  assert.deepEqual(await api.sendCommand('led_on'), { result: 'ok', output: 'on' });
  assert.equal(calls[1].path, '/api/command');
  assert.equal(calls[1].init.headers.Authorization, 'Bearer session-token');
  assert.deepEqual(JSON.parse(calls[1].init.body), { action: 'led_on' });
});

test('an authenticated call without a session fails before any request', async () => {
  const err = await rejection(api.getStatus());
  assert.equal(err.code, api.ErrorCode.INVALID_SESSION);
  assert.equal(calls.length, 0);
});

test('a 401 on an authenticated call drops the session', async () => {
  await signIn();
  respond(401, { error: 'invalid_session' });
  const err = await rejection(api.getStatus());
  assert.equal(err.code, api.ErrorCode.INVALID_SESSION);
  assert.equal(api.hasSession(), false);
});

test('a device-reported failure is distinct from an unconfirmed command', async () => {
  await signIn();
  respond(502, { error: 'command_failed', output: 'off' });
  const failed = await rejection(api.sendCommand('led_on'));
  assert.equal(failed.code, api.ErrorCode.COMMAND_FAILED);
  assert.equal(failed.status, 502);
  assert.equal(failed.reportedOutput, 'off');

  respond(504, { error: 'command_unconfirmed' });
  const unconfirmed = await rejection(api.sendCommand('led_on'));
  assert.equal(unconfirmed.code, api.ErrorCode.COMMAND_UNCONFIRMED);
  assert.equal(unconfirmed.reportedOutput, null);
});

test('a reported output outside on/off is ignored', async () => {
  await signIn();
  respond(502, { error: 'command_failed', output: '<b>on</b>' });
  const err = await rejection(api.sendCommand('led_on'));
  assert.equal(err.reportedOutput, null);
});

test('server_busy carries its retry hint', async () => {
  await signIn();
  respond(503, { error: 'server_busy', retry_after: 1 });
  const err = await rejection(api.getStatus());
  assert.equal(err.code, api.ErrorCode.SERVER_BUSY);
  assert.equal(err.retryAfter, 1);
});

test('status codes without a JSON body fall back to the documented codes', async () => {
  await signIn();
  for (const [status, code] of [
    [502, api.ErrorCode.TARGET_UNREACHABLE],
    [503, api.ErrorCode.LINK_DOWN],
    [504, api.ErrorCode.COMMAND_UNCONFIRMED],
    [429, api.ErrorCode.RATE_LIMITED],
    [418, api.ErrorCode.HTTP],
  ]) {
    respond(status);
    const err = await rejection(api.sendCommand('led_off'));
    assert.equal(err.code, code, `status ${status}`);
  }
});

test('transport failures and malformed success bodies have their own codes', async () => {
  await signIn();
  responses.push(() => Promise.reject(new TypeError('connection refused')));
  assert.equal((await rejection(api.getStatus())).code, api.ErrorCode.NETWORK);

  responses.push(() => Promise.reject(Object.assign(new Error('aborted'), { name: 'AbortError' })));
  assert.equal((await rejection(api.getStatus())).code, api.ErrorCode.TIMEOUT);

  respond(200, 'not an object');
  assert.equal((await rejection(api.getStatus())).code, api.ErrorCode.BAD_RESPONSE);

  respond(200, { token: 42, expires_in: 900 });
  api.clearSession();
  assert.equal((await rejection(api.login('operator', 'pw'))).code, api.ErrorCode.BAD_RESPONSE);
  assert.equal(api.hasSession(), false);
});
