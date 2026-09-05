// The toast is the only feedback a save gives. Every call used to schedule its
// own three-second timeout, so an earlier message hid the next one early, and it
// carried no live region, so it was never announced. This test pins both.
import assert from 'node:assert/strict';

import {
  adminSource, extractFunction, inlineScriptSafe, readRepoFile
} from '../../lib/admin-source.mjs';
import {runDomHarness} from '../../lib/headless-dom.mjs';

const html = readRepoFile("src/web/server/render/web_admin_html.cpp");
const css = readRepoFile('src', 'web', 'assets', 'admin.css');
const i18nHeader = readRepoFile("src/core/i18n/i18n.h");
const scripts = readRepoFile("src/web/server/render/web_admin_scripts.cpp");
const sensorHtml = readRepoFile('src', 'types', 'sensor', 'web_html.cpp');

assert.match(html, /id="notification"[^>]*role="status"[^>]*aria-live="polite"/,
  'The toast must be a live region so a save is announced');

// The state colors live in the stylesheet, not in an inline style.
assert.match(css, /\.notification\.is-error \{ background:var\(--notify-err\); \}/,
  'The error state must be a class, not an inline background');
assert.doesNotMatch(adminSource, /notification\.style\.background/,
  'showNotification must not set an inline background any more');
assert.match(css, /\.notification \{[^}]*transition:opacity 0\.3s, transform 0\.3s;/,
  'Only opacity and transform may transition, not the background');

// Icon-only reorder buttons need a localized accessible name.
for (const key of ['js_move_up', 'js_move_down']) {
  assert.ok(i18nHeader.includes(`const char* ${key};`),
    `${key} must exist in the central language schema`);
}
for (const entry of ['appendJsEntry("moveUp", tr.js_move_up)',
                     'appendJsEntry("moveDown", tr.js_move_down)']) {
  assert.ok(scripts.includes(entry),
    `The WebUI must receive the reorder label: ${entry}`);
}
for (const marker of ["up.setAttribute('aria-label', t('moveUp'))",
                      "down.setAttribute('aria-label', t('moveDown'))"]) {
  assert.ok(adminSource.includes(marker),
    `The reorder buttons need an accessible name: ${marker}`);
}

// The German example prefix used to reach English and French users. The Energy
// module already shows bare values in the same fields.
assert.doesNotMatch(sensorHtml, /placeholder="z\.\s?B\./,
  'Sensor placeholders must stay language neutral');

const harness = `<!doctype html><html><body>
  <div id="notification" class="notification" role="status" aria-live="polite"></div>
  <pre id="result">running</pre>
  <script>
  (() => {
    // Deterministic clock: collect the timers instead of waiting for them.
    const timers = new Map();
    let nextTimerId = 1;
    const realSetTimeout = window.setTimeout;
    window.setTimeout = (fn, delay) => {
      const id = nextTimerId++;
      timers.set(id, {fn, delay});
      return id;
    };
    window.clearTimeout = id => { timers.delete(id); };
    const runTimer = id => {
      const timer = timers.get(id);
      timers.delete(id);
      timer.fn();
    };

    ${inlineScriptSafe(extractFunction('showNotification'))}
    let notificationTimer = null;

    const toast = document.getElementById('notification');
    try {
      showNotification('Saved');
      if (!toast.classList.contains('show') ||
          toast.classList.contains('is-error') ||
          toast.textContent !== 'Saved') {
        throw new Error('A success toast must be visible and not marked as error');
      }
      if (timers.size !== 1) {
        throw new Error('One toast must arm exactly one timer');
      }
      const firstTimer = [...timers.keys()][0];

      // A second toast replaces the first, including its pending timeout.
      showNotification('Failed', false);
      if (timers.has(firstTimer)) {
        throw new Error('The previous timeout must be cleared, not left armed');
      }
      if (timers.size !== 1) {
        throw new Error('Only the newest toast may hold a timer');
      }
      if (!toast.classList.contains('is-error') ||
          toast.textContent !== 'Failed') {
        throw new Error('An error toast must replace the text and the state');
      }

      // Only the surviving timer hides the toast, and it clears the error state
      // for the next success message.
      runTimer([...timers.keys()][0]);
      if (toast.classList.contains('show')) {
        throw new Error('The toast must hide when its own timer fires');
      }
      showNotification('Saved again');
      if (toast.classList.contains('is-error')) {
        throw new Error('A later success must clear the error state');
      }

      // A page without the toast element must not throw.
      toast.remove();
      showNotification('No element');

      document.body.dataset.result = 'pass';
      document.getElementById('result').textContent = 'pass';
    } catch (error) {
      document.body.dataset.result = 'fail';
      document.getElementById('result').textContent = error.stack || String(error);
    } finally {
      window.setTimeout = realSetTimeout;
    }
  })();
  </script>
</body></html>`;

if (!runDomHarness({
  label: 'Admin notification DOM test',
  html: harness,
  tmpPrefix: 'hometiles-notification-'
})) {
  console.log('Admin notification source contract passed.');
}
