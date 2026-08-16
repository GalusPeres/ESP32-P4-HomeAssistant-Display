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
  assert.notEqual(bodyStart, -1, `${name} must have a body`);
  let depth = 0;
  for (let index = bodyStart; index < adminSource.length; index++) {
    const char = adminSource[index];
    if (char === '{') depth++;
    if (char !== '}') continue;
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
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    '/usr/bin/google-chrome',
    '/usr/bin/chromium',
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
  ].filter(Boolean);
  return candidates.find(candidate => fs.existsSync(candidate));
}

const browser = findBrowser();
assert.ok(browser, 'Chrome or Edge is required for the DOM drag regression test');

const productionFunctions = [
  extractFunction('restoreCurrentTileSelectionUi'),
  extractFunction('enableTileDrag'),
  extractFunction('enableSettingsHiddenSlot')
].join('\n\n');

const harness = `<!doctype html>
<html><body>
  <div id="tab-tiles-folder0">
    <div id="mainGrid" class="tile-grid">
      <div id="settingsGridTile" class="tile navigate active" draggable="true"
           data-index="0" data-type="7"><span id="gridSettingsChild">Settings</span></div>
      <div id="gridTargetTile" class="tile empty" draggable="true"
           data-index="1" data-type="0"><span id="gridTargetChild">Target</span></div>
    </div>
    <div id="settingsHiddenSlot" class="tile-grid settings-hidden-slot">
      <div id="settingsHiddenTile" class="tile settings-hidden-tile empty"
           draggable="false" data-hidden="0" data-span-w="1" data-span-h="1">
        <i id="parkingChild">Parking</i>
      </div>
      <span id="settingsHiddenHint"></span>
    </div>
  </div>
  <pre id="result">running</pre>
  <script>
  (() => {
    const GRID_COLS = 6;
    const GRID_ROWS = 5;
    const HIDDEN_SETTINGS_TILE_INDEX = -2;
    let resizeState = null;
    let dragSource = null;
    let dragPreview = null;
    let currentTileIndex = 0;
    let currentTileTab = 'folder0';
    let gridVisible = true;
    let hideCalls = 0;
    let restoreCalls = 0;
    let wrongGridDropCalls = 0;
    const gridTiles = [{type: 7}, {type: 0}];

    function getTileGrid() { return document.getElementById('mainGrid'); }
    function getTilesData() { return gridTiles; }
    function selectTile(index, tab) {
      currentTileIndex = index;
      currentTileTab = tab;
    }
    function getTileElementLayout(tab, index) {
      return {col: index, row: 0, span_w: 1, span_h: 1};
    }
    function getTileLayoutFromData(tab, index) {
      return getTileElementLayout(tab, index);
    }
    function getDragAnchorCell() { return {col: 0, row: 0}; }
    function getDragAnchorOffset() { return {x: 4, y: 4}; }
    function captureLayoutSnapshot() { return []; }
    function createDragPreview(tile) {
      const preview = tile.cloneNode(true);
      document.body.appendChild(preview);
      return preview;
    }
    function restoreDragPreview() {}
    function clearReflowPreviewClasses() {}
    function clearDragPlaceholder() {}
    function clearDeferredSensorRefresh() {}
    function flushDeferredSensorRefresh() {}
    function handleGridDragMove(tab, event) {
      if (!dragSource || dragSource.tab !== tab) return;
      event.preventDefault();
      if (dragSource.kind === 'hidden-settings') {
        dragSource.hiddenTarget = {col: 1, row: 0, span_w: 1, span_h: 1};
      }
    }
    function handleGridDrop(tab, event) {
      if (!dragSource || dragSource.tab !== tab) return;
      event.preventDefault();
      event.stopPropagation();
      if (dragSource.kind === 'hidden-settings') {
        dragSource.dropCommitted = true;
        restoreHiddenSettingsTile(1, 0);
      } else {
        wrongGridDropCalls++;
        dragSource.dropCommitted = true;
      }
    }
    function clampInt(value, min, max, fallback) {
      const number = Number.parseInt(value, 10);
      return Math.max(min, Math.min(max, Number.isFinite(number) ? number : fallback));
    }
    function settingsAccessElement() { return null; }
    function selectHiddenSettingsTile() {}
    function t(key) { return key; }
    function showNotification() {}

    function renderVisibility(hidden) {
      gridVisible = !hidden;
      currentTileIndex = hidden ? HIDDEN_SETTINGS_TILE_INDEX : 0;
      currentTileTab = 'folder0';
      gridTiles[0].type = hidden ? 0 : 7;
      const source = document.getElementById('settingsGridTile');
      source.dataset.type = hidden ? '0' : '7';
      source.draggable = !hidden;
      source.classList.toggle('active', !hidden);
      const parked = document.getElementById('settingsHiddenTile');
      parked.dataset.hidden = hidden ? '1' : '0';
      parked.dataset.type = hidden ? '7' : '0';
      parked.draggable = hidden;
      parked.classList.toggle('active', hidden);
      parked.style.background = hidden ? '#8b3434' : 'transparent';
    }
    function hideSettingsTileFromGrid() {
      hideCalls++;
      renderVisibility(true);
      return Promise.resolve(true);
    }
    function restoreHiddenSettingsTile() {
      restoreCalls++;
      renderVisibility(false);
      return Promise.resolve(true);
    }

    ${productionFunctions.replaceAll('</script', '<\\/script')}

    function transfer(source, target) {
      const dataTransfer = new DataTransfer();
      source.dispatchEvent(new DragEvent('dragstart', {
        bubbles: true, cancelable: true, dataTransfer, clientX: 20, clientY: 20
      }));
      target.dispatchEvent(new DragEvent('dragenter', {
        bubbles: true, cancelable: true, dataTransfer, clientX: 120, clientY: 20
      }));
      target.dispatchEvent(new DragEvent('dragover', {
        bubbles: true, cancelable: true, dataTransfer, clientX: 120, clientY: 20
      }));
      target.dispatchEvent(new DragEvent('drop', {
        bubbles: true, cancelable: true, dataTransfer, clientX: 120, clientY: 20
      }));
      source.dispatchEvent(new DragEvent('dragend', {
        bubbles: true, cancelable: true, dataTransfer, clientX: 120, clientY: 20
      }));
    }

    try {
      enableTileDrag('folder0');
      enableSettingsHiddenSlot();
      const gridSource = document.getElementById('settingsGridTile');
      const parkingChild = document.getElementById('parkingChild');
      const parkedSource = document.getElementById('settingsHiddenTile');
      const gridTargetChild = document.getElementById('gridTargetChild');

      for (let round = 0; round < 4; round++) {
        transfer(gridSource, parkingChild);
        if (hideCalls !== round + 1 || gridVisible || wrongGridDropCalls !== 0) {
          throw new Error('Grid to parking failed in round ' + (round + 1) +
            ': hide=' + hideCalls + ', visible=' + gridVisible +
            ', wrong-grid-drop=' + wrongGridDropCalls);
        }
        if (parkedSource.style.background !== 'rgb(139, 52, 52)' ||
            !parkedSource.classList.contains('active')) {
          throw new Error('Parking tile lost its color or active outline');
        }
        restoreCurrentTileSelectionUi();
        if (!parkedSource.classList.contains('active')) {
          throw new Error('Periodic preview refresh removed the parking selection');
        }

        transfer(parkedSource, gridTargetChild);
        if (restoreCalls !== round + 1 || !gridVisible) {
          throw new Error('Parking to grid failed in round ' + (round + 1));
        }
      }

      if (dragSource !== null || dragPreview !== null) {
        throw new Error('Drag lifecycle was not cleaned up');
      }
      document.body.dataset.result = 'pass';
      document.getElementById('result').textContent =
        'pass: four real DOM drag roundtrips through child targets';
    } catch (error) {
      document.body.dataset.result = 'fail';
      document.getElementById('result').textContent = error.stack || String(error);
    }
  })();
  </script>
</body></html>`;

const temporaryDirectory = fs.mkdtempSync(
  path.join(os.tmpdir(), 'hometiles-settings-drag-'));
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
    `DOM drag regression failed:\n${run.stdout}\n${run.stderr}`);
  console.log('Settings tile DOM drag roundtrip test passed.');
} finally {
  fs.rmSync(temporaryDirectory, {recursive: true, force: true});
}
