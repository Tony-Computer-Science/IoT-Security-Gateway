// Turns a monitoring history into strip segments. Pure: no DOM, so it is
// unit-tested in Node (tools/test/test_web_timeline.mjs).
//
// A history is the server's list of state transitions, oldest first:
//   [{ t: secondsSinceServiceStart, state: 'unknown' | 'up' | 'down' | 'off' }]
// The first point may lie before the window; it says which state was in
// effect when the window opens.

const STATES = new Set(['unknown', 'up', 'down', 'off']);

/**
 * @param {Array<{t: number, state: string}>} history
 * @param {number} nowS     server time of the snapshot (same base as t)
 * @param {number} windowS  width of the strip in seconds
 * @returns {Array<{state: string, startPct: number, widthPct: number}>}
 */
export function timelineSegments(history, nowS, windowS) {
  if (!Array.isArray(history) || !Number.isFinite(nowS) || !(windowS > 0)) return [];
  const start = nowS - windowS;
  const points = history
    .filter((p) => p && Number.isFinite(p.t) && p.t <= nowS)
    .map((p) => ({ t: Math.max(p.t, start), state: STATES.has(p.state) ? p.state : 'unknown' }));
  const segments = [];
  // Before the first known point (service just started) nothing was observed.
  if (points.length === 0 || points[0].t > start) {
    points.unshift({ t: start, state: 'unknown' });
  }
  for (let i = 0; i < points.length; i += 1) {
    const end = i + 1 < points.length ? points[i + 1].t : nowS;
    if (end <= points[i].t) continue;
    const last = segments[segments.length - 1];
    if (last && last.state === points[i].state) {
      last.end = end;
    } else {
      segments.push({ state: points[i].state, start: points[i].t, end });
    }
  }
  return segments.map((s) => ({
    state: s.state,
    startPct: ((s.start - start) / windowS) * 100,
    widthPct: ((s.end - s.start) / windowS) * 100,
  }));
}

/**
 * Seconds a component has spent down within the window, for a short summary.
 * @returns {number}
 */
export function downSeconds(history, nowS, windowS) {
  return timelineSegments(history, nowS, windowS)
    .filter((s) => s.state === 'down')
    .reduce((sum, s) => sum + (s.widthPct / 100) * windowS, 0);
}
