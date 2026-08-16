import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const adminSource = fs.readFileSync(
  path.join(root, 'src', 'web', 'assets', 'admin.js'), 'utf8');

function extractFunction(name) {
  const marker = `function ${name}(`;
  const start = adminSource.indexOf(marker);
  assert.notEqual(start, -1, `${name} must exist in admin.js`);
  const bodyStart = adminSource.indexOf('{', start + marker.length);
  let depth = 0;
  for (let index = bodyStart; index < adminSource.length; index++) {
    if (adminSource[index] === '{') depth++;
    if (adminSource[index] !== '}') continue;
    depth--;
    if (depth === 0) return adminSource.slice(start, index + 1);
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

const browser = findBrowser();
assert.ok(browser, 'Chrome or Edge is required for the folder PIN reveal test');

const productionFunctions = [
  extractFunction('togglePasswordVisibility'),
  extractFunction('loadNavigateFields')
].join('\n\n');

const harness = `<!doctype html><html><body>
  <input type="checkbox" id="folder0_folder_pin_enabled">
  <div class="password-field">
    <input type="password" id="folder0_folder_pin">
    <button class="password-toggle" data-label-show="Show"
            data-label-hide="Hide">Show</button>
  </div>
  <span id="folder0_folder_pin_status"></span>
  <pre id="result">running</pre>
  <script>
  (() => {
    function syncFolderPinControls() {}
    function navigateText(key) { return key === 'folderPinSaved' ? 'PIN saved' : key; }
    ${productionFunctions.replaceAll('</script', '<\\/script')}
    try {
      const input = document.getElementById('folder0_folder_pin');
      const button = document.querySelector('.password-toggle');
      const status = document.getElementById('folder0_folder_pin_status');
      loadNavigateFields('folder0', {
        folder_pin_enabled: true,
        folder_pin: '1234'
      });
      if (input.value !== '1234' || input.type !== 'password' ||
          button.textContent !== 'Show' || status.textContent !== 'PIN saved') {
        throw new Error('Stored PIN was not restored in masked form');
      }
      togglePasswordVisibility('folder0_folder_pin', button);
      if (input.type !== 'text' || input.value !== '1234' ||
          button.textContent !== 'Hide') {
        throw new Error('Show did not reveal the restored PIN');
      }
      loadNavigateFields('folder0', {
        folder_pin_enabled: true,
        folder_pin: '1234'
      });
      if (input.type !== 'password' || input.value !== '1234' ||
          button.textContent !== 'Show') {
        throw new Error('Returning to the folder did not restore masked state');
      }
      loadNavigateFields('folder0', {
        folder_pin_enabled: true,
        folder_pin: ''
      });
      if (input.value !== '' || input.type !== 'password') {
        throw new Error('Legacy hash-only PIN must remain protected and blank');
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

const temporaryDirectory = fs.mkdtempSync(
  path.join(os.tmpdir(), 'hometiles-folder-pin-'));
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
    '--dump-dom',
    pathToFileURL(harnessPath).href
  ], {encoding: 'utf8', timeout: 30000});
  assert.equal(run.error, undefined, run.error?.message);
  assert.equal(run.status, 0, run.stderr || run.stdout);
  assert.match(run.stdout, /data-result="pass"/,
    `Folder PIN reveal regression failed:\n${run.stdout}\n${run.stderr}`);
  console.log('Folder PIN reveal DOM test passed.');
} finally {
  fs.rmSync(temporaryDirectory, {recursive: true, force: true});
}
