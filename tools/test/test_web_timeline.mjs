// Behavior tests for web/src/timeline.js (monitoring strips).
// Run: node --test tools/test/test_web_timeline.mjs
import assert from 'node:assert/strict';
import { test } from 'node:test';

import { downSeconds, timelineSegments } from '../../web/src/timeline.js';

const round = (segments) => segments.map((s) => ({
  state: s.state, startPct: Math.round(s.startPct * 10) / 10, widthPct: Math.round(s.widthPct * 10) / 10,
}));

test('a fresh service shows unobserved time before its first point', () => {
  // Service started 60 s ago; window is 300 s.
  const segments = round(timelineSegments([{ t: 0, state: 'unknown' }, { t: 30, state: 'up' }], 60, 300));
  assert.deepEqual(segments, [
    { state: 'unknown', startPct: 0, widthPct: 90 },
    { state: 'up', startPct: 90, widthPct: 10 },
  ]);
});

test('a point before the window is clipped to the window start', () => {
  const segments = round(timelineSegments(
    [{ t: 100, state: 'up' }, { t: 500, state: 'down' }, { t: 560, state: 'up' }], 600, 300));
  assert.deepEqual(segments, [
    { state: 'up', startPct: 0, widthPct: 66.7 },
    { state: 'down', startPct: 66.7, widthPct: 20 },
    { state: 'up', startPct: 86.7, widthPct: 13.3 },
  ]);
  assert.equal(Math.round(downSeconds(
    [{ t: 100, state: 'up' }, { t: 500, state: 'down' }, { t: 560, state: 'up' }], 600, 300)), 60);
});

test('segments always cover the window exactly once', () => {
  const history = [{ t: 0, state: 'unknown' }, { t: 10, state: 'up' }, { t: 200, state: 'down' },
    { t: 210, state: 'up' }, { t: 290, state: 'off' }];
  const segments = timelineSegments(history, 300, 300);
  const total = segments.reduce((sum, s) => sum + s.widthPct, 0);
  assert.ok(Math.abs(total - 100) < 1e-9);
  for (let i = 1; i < segments.length; i += 1) {
    assert.ok(Math.abs(segments[i].startPct - (segments[i - 1].startPct + segments[i - 1].widthPct)) < 1e-9);
    assert.notEqual(segments[i].state, segments[i - 1].state);
  }
});

test('bad input degrades to unknown or nothing, never throws', () => {
  assert.deepEqual(timelineSegments(null, 10, 300), []);
  assert.deepEqual(timelineSegments([], Number.NaN, 300), []);
  assert.deepEqual(timelineSegments([], 10, 0), []);
  assert.deepEqual(round(timelineSegments([{ t: 0, state: '<img>' }, { t: 'x', state: 'up' }], 300, 300)),
    [{ state: 'unknown', startPct: 0, widthPct: 100 }]);
  // Points from the future (clock skew) are ignored.
  assert.deepEqual(round(timelineSegments([{ t: 0, state: 'up' }, { t: 999, state: 'down' }], 300, 300)),
    [{ state: 'up', startPct: 0, widthPct: 100 }]);
});
