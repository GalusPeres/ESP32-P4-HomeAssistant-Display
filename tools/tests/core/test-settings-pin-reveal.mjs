import assert from 'node:assert/strict';

import {extractDeliveredFunction, inlineScriptSafe, readRepoFile} from '../../lib/admin-source.mjs';
import {runDomHarness} from '../../lib/headless-dom.mjs';

const settingsHtml = readRepoFile('src', 'types', 'settings', 'web_html.cpp');

for (const marker of [
  'configManager.getSettingsPin(stored_pin);',
  'appendHtmlEscaped(html, stored_pin);',
]) {
  assert.ok(settingsHtml.includes(marker),
    `Settings server-rendered PIN field is missing: ${marker}`);
}

const productionFunctions = [
  extractDeliveredFunction('togglePasswordVisibility'),
  extractDeliveredFunction('saveSettingsAccess')
].join('\n\n');

const harness = `<!doctype html><html><body>
  <input type="checkbox" id="folder0_settings_pin_enabled"
         data-pin-configured="1" checked>
  <input type="checkbox" id="folder0_settings_tile_hidden">
  <input type="checkbox" id="folder0_settings_swipe_enabled">
  <select id="folder0_settings_reveal_edge"><option value="0">Left</option></select>
  <div id="folder0_settings_pin_fields">
    <div class="password-field">
      <input type="password" id="folder0_settings_pin" value="2468">
      <button class="password-toggle" data-label-show="Show"
              data-label-hide="Hide">Show</button>
    </div>
    <button id="folder0_settings_pin_apply">Apply PIN</button>
  </div>
  <span id="folder0_settings_pin_status" data-configured-text="PIN saved."
        data-not-configured-text="No PIN"></span>
  <pre id="result">running</pre>
  <script>
  (() => {
    const SETTINGS_ACCESS_PREFIX = 'folder0_';
    const settingsAccessElement = suffix =>
      document.getElementById(SETTINGS_ACCESS_PREFIX + suffix);
    let settingsAccessCommittedState = {
      pinEnabled: true, pinConfigured: true, tileHidden: false,
      swipeEnabled: false, revealEdge: '0'
    };
    function readSettingsAccessState() { return {...settingsAccessCommittedState}; }
    function currentGridSettingsSnapshot() { return {type: '7'}; }
    function normalizeHiddenSettingsSnapshot(source = null) { return source || {type: '7'}; }
    function normalizeSnapshotLayout() { return {col: 1, row: 1, span_w: 1, span_h: 1}; }
    function tileBgValueIsSet() { return false; }
    function hexToRgb() { return 0; }
    function toggleSettingsAccessFields() {}
    function settingsAccessStatesEqual() { return true; }
    function restoreSettingsAccessState() {}
    function showNotification(message, ok = true) {
      if (!ok) throw new Error(message);
    }
    function t(key) { return key; }
    function setSettingsPinStatus(configured) {
      const toggle = settingsAccessElement('settings_pin_enabled');
      const status = settingsAccessElement('settings_pin_status');
      toggle.dataset.pinConfigured = configured ? '1' : '0';
      status.textContent = configured
        ? status.dataset.configuredText : status.dataset.notConfiguredText;
    }
    async function reconcileSettingsTileUi() { return true; }
    let responseSettingsPin = '2468';
    window.fetch = async () => ({
      ok: true,
      json: async () => ({
        ok: true, reload: false, settings_pin: responseSettingsPin
      })
    });
    ${inlineScriptSafe(productionFunctions)}
    (async () => {
      try {
        const input = settingsAccessElement('settings_pin');
        const button = document.querySelector('.password-toggle');
        const requested = {...settingsAccessCommittedState};
        const saved = await saveSettingsAccess(
          '2468', null, null, requested, false);
        if (!saved || input.value !== '2468' || input.type !== 'password' ||
            button.textContent !== 'Show') {
          throw new Error('Saved Settings PIN was not restored in masked form');
        }
        togglePasswordVisibility('folder0_settings_pin', button);
        if (input.type !== 'text' || input.value !== '2468' ||
            button.textContent !== 'Hide') {
          throw new Error('Show did not reveal the Settings PIN');
        }
        await saveSettingsAccess(null, null, null, requested, false);
        if (input.type !== 'password' || input.value !== '2468' ||
            button.textContent !== 'Show') {
          throw new Error('A later access save did not restore masked state');
        }
        responseSettingsPin = '';
        input.value = '1357';
        await saveSettingsAccess(null, null, null, requested, false);
        if (input.value !== '1357') {
          throw new Error('A legacy hash-only response erased an unsaved PIN');
        }
        document.body.dataset.result = 'pass';
        document.getElementById('result').textContent = 'pass';
      } catch (error) {
        document.body.dataset.result = 'fail';
        document.getElementById('result').textContent = error.stack || String(error);
      }
    })();
  })();
  </script>
</body></html>`;

runDomHarness({
  label: 'Settings PIN reveal DOM test',
  html: harness,
  tmpPrefix: 'hometiles-settings-pin-',
  extraArgs: ['--virtual-time-budget=1000']
});
