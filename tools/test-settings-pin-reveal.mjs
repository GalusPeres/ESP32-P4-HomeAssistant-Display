import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const adminSource = fs.readFileSync(
  path.join(root, 'src', 'web', 'assets', 'admin.js'), 'utf8');
const settingsHtml = fs.readFileSync(
  path.join(root, 'src', 'types', 'settings', 'web_html.cpp'), 'utf8');

function extractFunction(name) {
  const marker = `function ${name}(`;
  const start = adminSource.indexOf(marker);
  assert.notEqual(start, -1, `${name} must exist in admin.js`);
  const declarationStart =
    adminSource.slice(Math.max(0, start - 6), start) === 'async '
      ? start - 6
      : start;
  const bodyStart = adminSource.indexOf('{', start + marker.length);
  let depth = 0;
  for (let index = bodyStart; index < adminSource.length; index++) {
    if (adminSource[index] === '{') depth++;
    if (adminSource[index] !== '}') continue;
    depth--;
    if (depth === 0) return adminSource.slice(declarationStart, index + 1);
  }
  assert.fail(`${name} body is incomplete`);
}

function findBrowser() {
  const candidates = [
    process.env.CHROME_PATH,
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe'
  ].filter(Boolean);
  return candidates.find(candidate => fs.existsSync(candidate));
}

for (const marker of [
  'configManager.getSettingsPin(stored_pin);',
  'appendHtmlEscaped(html, stored_pin);',
]) {
  assert.ok(settingsHtml.includes(marker),
    `Settings server-rendered PIN field is missing: ${marker}`);
}

const browser = findBrowser();
assert.ok(browser, 'Chrome or Edge is required for the Settings PIN test');

const productionFunctions = [
  extractFunction('togglePasswordVisibility'),
  extractFunction('saveSettingsAccess')
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
    ${productionFunctions.replaceAll('</script', '<\\/script')}
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

const temporaryDirectory = fs.mkdtempSync(
  path.join(os.tmpdir(), 'hometiles-settings-pin-'));
const harnessPath = path.join(temporaryDirectory, 'index.html');
try {
  fs.writeFileSync(harnessPath, harness, 'utf8');
  const run = spawnSync(browser, [
    '--headless=new',
    '--disable-gpu',
    '--disable-extensions',
    '--no-first-run',
    '--no-default-browser-check',
    '--disable-background-networking',
    '--virtual-time-budget=1000',
    '--dump-dom',
    pathToFileURL(harnessPath).href
  ], {encoding: 'utf8', timeout: 30000});
  assert.equal(run.error, undefined, run.error?.message);
  assert.equal(run.status, 0, run.stderr || run.stdout);
  assert.match(run.stdout, /data-result="pass"/,
    `Settings PIN reveal regression failed:\n${run.stdout}\n${run.stderr}`);
  console.log('Settings PIN reveal DOM test passed.');
} finally {
  fs.rmSync(temporaryDirectory, {recursive: true, force: true});
}
