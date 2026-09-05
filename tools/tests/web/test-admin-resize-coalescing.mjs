// Both window resize handlers are expensive: updateTileSettingsMaxHeight forces
// a layout and reads computed styles per panel, and the screensaver handler
// re-renders the whole editor. Resize fires many times per second while a window
// is dragged, so the work is coalesced to one call per animation frame.
import assert from 'node:assert/strict';

import {
  adminSource, extractDeliveredFunction, inlineScriptSafe
} from '../../lib/admin-source.mjs';
import {runDomHarness} from '../../lib/headless-dom.mjs';

for (const marker of [
  "window.addEventListener('resize', perFrame(updateTileSettingsMaxHeight));",
  "window.addEventListener('resize', perFrame(() => {"
]) {
  assert.ok(adminSource.includes(marker),
    `The resize handlers must stay coalesced: ${marker}`);
}
// No resize listener may call an expensive handler directly any more. Counting
// both forms avoids a lookahead that whitespace could slip past.
const resizeListeners =
  (adminSource.match(/addEventListener\('resize',/g) || []).length;
const coalescedListeners =
  (adminSource.match(/addEventListener\('resize', perFrame\(/g) || []).length;
assert.ok(resizeListeners > 0, 'The Admin must keep its resize listeners');
assert.equal(coalescedListeners, resizeListeners,
  'Every resize listener must go through perFrame');

const harness = `<!doctype html><html><body>
  <pre id="result">running</pre>
  <script>
  (() => {
    // Drive the frame callbacks by hand so the test is deterministic.
    const frames = [];
    let nextFrame = 1;
    window.requestAnimationFrame = callback => {
      const id = nextFrame++;
      frames.push({id, callback});
      return id;
    };
    const flushFrame = () => {
      const frame = frames.shift();
      if (frame) frame.callback();
    };

    ${inlineScriptSafe(extractDeliveredFunction('perFrame'))}
    try {
      let calls = 0;
      const handler = perFrame(() => { calls++; });

      // A burst of events must schedule exactly one frame.
      for (let i = 0; i < 25; i++) handler();
      if (frames.length !== 1) {
        throw new Error('A burst must schedule one frame, got ' + frames.length);
      }
      if (calls !== 0) {
        throw new Error('The work must wait for the frame, not run inline');
      }

      flushFrame();
      if (calls !== 1) {
        throw new Error('The frame must run the work exactly once, got ' + calls);
      }

      // After the frame ran, a later burst schedules again.
      for (let i = 0; i < 10; i++) handler();
      if (frames.length !== 1) {
        throw new Error('A later burst must schedule again');
      }
      flushFrame();
      if (calls !== 2) {
        throw new Error('The second burst must run once, got ' + calls);
      }

      // Two coalesced handlers stay independent.
      let other = 0;
      const second = perFrame(() => { other++; });
      handler();
      second();
      if (frames.length !== 2) {
        throw new Error('Independent handlers must each schedule their own frame');
      }
      flushFrame();
      flushFrame();
      if (calls !== 3 || other !== 1) {
        throw new Error('Each handler must run its own work: ' + calls + '/' + other);
      }

      document.body.dataset.result = 'pass';
      document.getElementById('result').textContent = 'pass';
    } catch (error) {
      document.body.dataset.result = 'fail';
      document.getElementById('result').textContent = error.stack || String(error);
    }
  })();
  </script>
</body></html>`;

if (!runDomHarness({
  label: 'Admin resize coalescing DOM test',
  html: harness,
  tmpPrefix: 'hometiles-resize-coalescing-'
})) {
  console.log('Admin resize coalescing source contract passed.');
}
