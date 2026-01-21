#include "src/web/web_admin_scripts.h"
#include "src/types/image/web_scripts.h"
#include "src/types/key/web_scripts.h"
#include "src/types/navigate/web_scripts.h"
#include "src/types/scene/web_scripts.h"
#include "src/types/sensor/web_scripts.h"
#include "src/types/switch/web_scripts.h"
#include "src/types/clock/web_scripts.h"

void appendAdminScripts(String& html) {
  html += R"html(
  <script>
  function switchTab(tabName) {
    const tabs = document.querySelectorAll('.tab-content');
    tabs.forEach(tab => tab.classList.remove('active'));
    const btns = document.querySelectorAll('.tab-btn');
    btns.forEach(btn => btn.classList.remove('active'));
    const target = document.getElementById(tabName);
    if (target) target.classList.add('active');
    // Find and activate the button that switches to this tab
    const activeBtn = Array.from(btns).find(btn => btn.getAttribute('onclick')?.includes("'" + tabName + "'"));
    if (activeBtn) activeBtn.classList.add('active');
    try { localStorage.setItem('activeAdminTab', tabName); } catch (e) {}
  }

  // Tile Editor State
  const GRID_COLS = 6;
  const GRID_ROWS = 4;
  const tileTabs = [];
  const folderByTab = {};
  const tabByFolder = {};
  let currentTileTab = '';
  let currentTileIndex = -1;
  let drafts = {};
  let tilesData = {};
  let autoSaveTimers = {};

  function initTileTabs() {
    tileTabs.length = 0;
    Object.keys(folderByTab).forEach(k => delete folderByTab[k]);
    Object.keys(tabByFolder).forEach(k => delete tabByFolder[k]);
    document.querySelectorAll('.tile-tab').forEach(tabEl => {
      const tabId = tabEl.dataset.tabId || '';
      if (!tabId) return;
      tileTabs.push(tabId);
      const folderId = parseInt(tabEl.dataset.folderId, 10);
      if (!isNaN(folderId)) {
        folderByTab[tabId] = folderId;
        tabByFolder[folderId] = tabId;
      }
      if (!drafts[tabId]) drafts[tabId] = {};
      if (!tilesData[tabId]) tilesData[tabId] = [];
      if (!autoSaveTimers[tabId]) autoSaveTimers[tabId] = null;
    });
    if (!currentTileTab && tileTabs.length) currentTileTab = tileTabs[0];
  }

  function getFolderIdForTab(tab) {
    return folderByTab[tab];
  }

  function getTilesData(tab) {
    return tilesData[tab] || [];
  }
  function clampInt(value, min, max, fallback) {
    const v = parseInt(value, 10);
    if (isNaN(v)) return fallback !== undefined ? fallback : min;
    if (v < min) return min;
    if (v > max) return max;
    return v;
  }

  function normalizeTileLayout(tile, index) {
    const fallbackCol = index % GRID_COLS;
    const fallbackRow = Math.floor(index / GRID_COLS);
    const col = clampInt(tile?.col, 0, GRID_COLS - 1, fallbackCol);
    const row = clampInt(tile?.row, 0, GRID_ROWS - 1, fallbackRow);
    let spanW = clampInt(tile?.span_w, 1, GRID_COLS, 1);
    let spanH = clampInt(tile?.span_h, 1, GRID_ROWS, 1);
    if (spanW > GRID_COLS - col) spanW = GRID_COLS - col;
    if (spanH > GRID_ROWS - row) spanH = GRID_ROWS - row;
    return { col, row, span_w: spanW, span_h: spanH };
  }

  function setTileGridPosition(el, col, row, spanW, spanH) {
    if (!el) return;
    el.style.gridColumn = (col + 1) + ' / span ' + spanW;
    el.style.gridRow = (row + 1) + ' / span ' + spanH;
    el.dataset.col = String(col);
    el.dataset.row = String(row);
    el.dataset.spanW = String(spanW);
    el.dataset.spanH = String(spanH);
  }

  function getTileElementLayout(tab, index) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return null;
    const col = clampInt(el.dataset.col, 0, GRID_COLS - 1, null);
    const row = clampInt(el.dataset.row, 0, GRID_ROWS - 1, null);
    const spanW = clampInt(el.dataset.spanW, 1, GRID_COLS, null);
    const spanH = clampInt(el.dataset.spanH, 1, GRID_ROWS, null);
    if (col === null || row === null || spanW === null || spanH === null) return null;
    return { col, row, span_w: spanW, span_h: spanH };
  }

  function layoutTiles(tab, tiles) {
    if (!Array.isArray(tiles)) return;
    const occupied = Array.from({ length: GRID_ROWS }, () => Array(GRID_COLS).fill(false));
    const emptyIndices = [];

    tiles.forEach((tile, idx) => {
      const typeNum = Number(tile?.type);
      if (!tile || isNaN(typeNum) || typeNum === 0) {
        emptyIndices.push(idx);
        return;
      }
      const layout = normalizeTileLayout(tile, idx);
      const el = document.getElementById(tab + '-tile-' + idx);
      if (el) {
        setTileGridPosition(el, layout.col, layout.row, layout.span_w, layout.span_h);
        el.style.display = '';
      }
      for (let r = layout.row; r < layout.row + layout.span_h; r++) {
        for (let c = layout.col; c < layout.col + layout.span_w; c++) {
          if (r < GRID_ROWS && c < GRID_COLS) occupied[r][c] = true;
        }
      }
    });

    const freeCells = [];
    for (let r = 0; r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        if (!occupied[r][c]) freeCells.push({ col: c, row: r });
      }
    }

    emptyIndices.forEach((idx, i) => {
      const el = document.getElementById(tab + '-tile-' + idx);
      if (!el) return;
      if (i < freeCells.length) {
        const cell = freeCells[i];
        setTileGridPosition(el, cell.col, cell.row, 1, 1);
        el.style.display = '';
      } else {
        el.style.display = 'none';
      }
    });
  }

  function normalizeLayoutInputs(tab) {
    const prefix = tab;
    const colEl = document.getElementById(prefix + '_tile_col');
    const rowEl = document.getElementById(prefix + '_tile_row');
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    const spanHEl = document.getElementById(prefix + '_tile_span_h');

    if (!colEl || !rowEl || !spanWEl || !spanHEl) {
      const fallback = getTileElementLayout(tab, currentTileIndex);
      if (fallback) return fallback;
      return { col: 0, row: 0, span_w: 1, span_h: 1 };
    }

    let col = clampInt(colEl.value, 1, GRID_COLS, 1);
    let row = clampInt(rowEl.value, 1, GRID_ROWS, 1);
    let spanW = clampInt(spanWEl.value, 1, GRID_COLS, 1);
    let spanH = clampInt(spanHEl.value, 1, GRID_ROWS, 1);

    const maxSpanW = GRID_COLS - (col - 1);
    const maxSpanH = GRID_ROWS - (row - 1);
    if (spanW > maxSpanW) spanW = maxSpanW;
    if (spanH > maxSpanH) spanH = maxSpanH;

    colEl.value = String(col);
    rowEl.value = String(row);
    spanWEl.value = String(spanW);
    spanHEl.value = String(spanH);

    return { col: col - 1, row: row - 1, span_w: spanW, span_h: spanH };
  }

  function updateLayoutFromInputs(tab) {
    if (currentTileIndex === -1) return;
    const layout = normalizeLayoutInputs(tab);
    const tiles = getTilesData(tab);
    const tileEl = document.getElementById(tab + '-tile-' + currentTileIndex);
    if (tileEl && (!Array.isArray(tiles) || tiles.length === 0)) {
      setTileGridPosition(tileEl, layout.col, layout.row, layout.span_w, layout.span_h);
      return;
    }
    if (!Array.isArray(tiles) || currentTileIndex >= tiles.length) return;
    const tile = tiles[currentTileIndex] || {};
    tile.col = layout.col;
    tile.row = layout.row;
    tile.span_w = layout.span_w;
    tile.span_h = layout.span_h;
    const typeEl = document.getElementById(tab + '_tile_type');
    const typeNum = typeEl ? parseInt(typeEl.value, 10) : 0;
    tile.type = isNaN(typeNum) ? 0 : typeNum;
    tiles[currentTileIndex] = tile;
    layoutTiles(tab, tiles);
  }

  function applyLayoutInputsFromLayout(tab, layout) {
    if (!layout) return;
    const colEl = document.getElementById(tab + '_tile_col');
    const rowEl = document.getElementById(tab + '_tile_row');
    const colVal = String(layout.col + 1);
    const rowVal = String(layout.row + 1);
    if (colEl) colEl.value = colVal;
    if (rowEl) rowEl.value = rowVal;
    const tabDrafts = drafts[tab];
    if (tabDrafts && tabDrafts[currentTileIndex]) {
      tabDrafts[currentTileIndex].col = colVal;
      tabDrafts[currentTileIndex].row = rowVal;
      persistDrafts();
    }
  }

  function persistDrafts() { try { localStorage.setItem('tileDrafts', JSON.stringify(drafts)); } catch (e) {} }
  function loadDraftsFromStorage() {
    try {
      const raw = localStorage.getItem('tileDrafts');
      if (raw) {
        drafts = JSON.parse(raw);
        // Drafts sollen nach Page-Refresh nicht gespeicherte Werte ueberschreiben.
        for (const tab in drafts) {
          const tabDrafts = drafts[tab];
          if (!tabDrafts) continue;
          Object.keys(tabDrafts).forEach(key => {
            if (tabDrafts[key]) tabDrafts[key]._dirty = false;
          });
        }
      }
    } catch (e) {
      drafts = {};
    }
  }
  function clearDraft(tab, index) {
    if (drafts[tab] && drafts[tab][index]) {
      delete drafts[tab][index];
      persistDrafts();
    }
  }

  function swapDrafts(tab, fromIdx, toIdx) {
    if (!drafts[tab]) return;
    const fromVal = drafts[tab][fromIdx];
    const toVal = drafts[tab][toIdx];
    if (fromVal === undefined && toVal === undefined) return;
    drafts[tab][toIdx] = fromVal;
    if (toVal === undefined) delete drafts[tab][fromIdx];
    else drafts[tab][fromIdx] = toVal;
    persistDrafts();
  }

  function updateDraft(tab) {
    if (currentTileIndex === -1) return;
    if (!drafts[tab]) drafts[tab] = {};
    const prefix = tab;
    const d = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: document.getElementById(prefix + '_tile_color')?.value || '#2A2A2A',
      col: document.getElementById(prefix + '_tile_col')?.value || '1',
      row: document.getElementById(prefix + '_tile_row')?.value || '1',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1',
      sensor_entity: document.getElementById(prefix + '_sensor_entity')?.value || '',
      sensor_unit: document.getElementById(prefix + '_sensor_unit')?.value || '',
      sensor_decimals: document.getElementById(prefix + '_sensor_decimals')?.value || '',
      sensor_value_font: document.getElementById(prefix + '_sensor_value_font')?.value || '0',
      sensor_gauge: document.getElementById(prefix + '_sensor_gauge')?.checked ? '1' : '0',
      sensor_gauge_min: document.getElementById(prefix + '_sensor_gauge_min')?.value || '',
      sensor_gauge_max: document.getElementById(prefix + '_sensor_gauge_max')?.value || '',
      scene_alias: document.getElementById(prefix + '_scene_alias')?.value || '',
      key_macro: document.getElementById(prefix + '_key_macro')?.value || '',
      navigate_target: document.getElementById(prefix + '_navigate_target')?.value || '0',
      switch_entity: document.getElementById(prefix + '_switch_entity')?.value || '',
      switch_style: document.getElementById(prefix + '_switch_style')?.value || '0',
      image_path: document.getElementById(prefix + '_image_path')?.value || '',
      image_slideshow_sec: document.getElementById(prefix + '_image_slideshow_sec')?.value || '10',
      clock_flags: getClockFlagsFromInputs(prefix),
      _dirty: true
    };
    drafts[tab][currentTileIndex] = d;
    persistDrafts();
  }

  function applyDraft(tab, index) {
    const d = drafts[tab] && drafts[tab][index];
    if (!d || !d._dirty) return false;
    const prefix = tab;
    document.getElementById(prefix + '_tile_type').value = d.type || '0';
    updateTileType(tab);
    document.getElementById(prefix + '_tile_title').value = d.title || '';
    document.getElementById(prefix + '_tile_icon').value = d.icon || '';
    document.getElementById(prefix + '_tile_color').value = d.color || '#2A2A2A';
    const colEl = document.getElementById(prefix + '_tile_col');
    if (colEl) colEl.value = d.col || '1';
    const rowEl = document.getElementById(prefix + '_tile_row');
    if (rowEl) rowEl.value = d.row || '1';
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    if (spanWEl) spanWEl.value = d.span_w || '1';
    const spanHEl = document.getElementById(prefix + '_tile_span_h');
    if (spanHEl) spanHEl.value = d.span_h || '1';
    if (d.type === '1') {
      document.getElementById(prefix + '_sensor_entity').value = d.sensor_entity || '';
      document.getElementById(prefix + '_sensor_unit').value = d.sensor_unit || '';
      const decEl = document.getElementById(prefix + '_sensor_decimals');
      if (decEl) decEl.value = d.sensor_decimals || '';
      const fontEl = document.getElementById(prefix + '_sensor_value_font');
      if (fontEl) fontEl.value = d.sensor_value_font || '0';
      const gaugeEl = document.getElementById(prefix + '_sensor_gauge');
      if (gaugeEl) gaugeEl.checked = d.sensor_gauge === '1';
      const gaugeMinEl = document.getElementById(prefix + '_sensor_gauge_min');
      if (gaugeMinEl) gaugeMinEl.value = d.sensor_gauge_min || '';
      const gaugeMaxEl = document.getElementById(prefix + '_sensor_gauge_max');
      if (gaugeMaxEl) gaugeMaxEl.value = d.sensor_gauge_max || '';
      syncGaugeUi(tab);
    } else if (d.type === '2') {
      document.getElementById(prefix + '_scene_alias').value = d.scene_alias || '';
    } else if (d.type === '3') {
      document.getElementById(prefix + '_key_macro').value = d.key_macro || '';
    } else if (d.type === '4') {
      const navEl = document.getElementById(prefix + '_navigate_target');
      if (navEl) navEl.value = d.navigate_target || '0';
    } else if (d.type === '5') {
      document.getElementById(prefix + '_switch_entity').value = d.switch_entity || '';
      const styleEl = document.getElementById(prefix + '_switch_style');
      if (styleEl) styleEl.value = d.switch_style || '0';
    } else if (d.type === '6') {
      document.getElementById(prefix + '_image_path').value = d.image_path || '';
      const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
      if (intervalEl && d.image_slideshow_sec !== undefined && d.image_slideshow_sec !== null && String(d.image_slideshow_sec).length > 0) {
        intervalEl.value = d.image_slideshow_sec;
      }
      applyImageUiState(tab, d.image_path || '');
      refreshImageSelect(tab, false);
    } else if (d.type === '9') {
      applyClockFlagsToInputs(prefix, d.clock_flags);
    }
    updateTilePreview(tab);
    return true;
  }

  let tileClipboard = null;
  function persistTileClipboard() { try { localStorage.setItem('tileClipboard', JSON.stringify(tileClipboard)); } catch (e) {} }
  function loadTileClipboard() {
    try {
      const raw = localStorage.getItem('tileClipboard');
      if (raw) tileClipboard = JSON.parse(raw);
    } catch (e) {
      tileClipboard = null;
    }
  }

  function collectTileFormData(tab) {
    const prefix = tab;
    return {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: document.getElementById(prefix + '_tile_color')?.value || '#2A2A2A',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1',
      sensor_entity: document.getElementById(prefix + '_sensor_entity')?.value || '',
      sensor_unit: document.getElementById(prefix + '_sensor_unit')?.value || '',
      sensor_decimals: document.getElementById(prefix + '_sensor_decimals')?.value || '',
      sensor_value_font: document.getElementById(prefix + '_sensor_value_font')?.value || '0',
      sensor_gauge: document.getElementById(prefix + '_sensor_gauge')?.checked ? '1' : '0',
      sensor_gauge_min: document.getElementById(prefix + '_sensor_gauge_min')?.value || '',
      sensor_gauge_max: document.getElementById(prefix + '_sensor_gauge_max')?.value || '',
      scene_alias: document.getElementById(prefix + '_scene_alias')?.value || '',
      key_macro: document.getElementById(prefix + '_key_macro')?.value || '',
      navigate_target: document.getElementById(prefix + '_navigate_target')?.value || '0',
      switch_entity: document.getElementById(prefix + '_switch_entity')?.value || '',
      switch_style: document.getElementById(prefix + '_switch_style')?.value || '0',
      image_path: document.getElementById(prefix + '_image_path')?.value || '',
      image_slideshow_sec: document.getElementById(prefix + '_image_slideshow_sec')?.value || '10',
      clock_flags: getClockFlagsFromInputs(prefix)
    };
  }

  function applyTileFormData(tab, data) {
    if (!data) return;
    const prefix = tab;
    const typeValue = data.type || '0';
    const typeEl = document.getElementById(prefix + '_tile_type');
    if (typeEl) typeEl.value = typeValue;
    updateTileType(tab);

    const titleEl = document.getElementById(prefix + '_tile_title');
    if (titleEl) titleEl.value = data.title || '';
    const iconEl = document.getElementById(prefix + '_tile_icon');
    if (iconEl) iconEl.value = data.icon || '';
    const colorEl = document.getElementById(prefix + '_tile_color');
    if (colorEl) colorEl.value = data.color || '#2A2A2A';
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    if (spanWEl) spanWEl.value = data.span_w || '1';
    const spanHEl = document.getElementById(prefix + '_tile_span_h');
    if (spanHEl) spanHEl.value = data.span_h || '1';

    const sensorEntityEl = document.getElementById(prefix + '_sensor_entity');
    if (sensorEntityEl) sensorEntityEl.value = data.sensor_entity || '';
    const sensorUnitEl = document.getElementById(prefix + '_sensor_unit');
    if (sensorUnitEl) sensorUnitEl.value = data.sensor_unit || '';
    const sensorDecEl = document.getElementById(prefix + '_sensor_decimals');
    if (sensorDecEl) sensorDecEl.value = data.sensor_decimals || '';
    const sensorFontEl = document.getElementById(prefix + '_sensor_value_font');
    if (sensorFontEl) sensorFontEl.value = data.sensor_value_font || '0';
    const sensorGaugeEl = document.getElementById(prefix + '_sensor_gauge');
    if (sensorGaugeEl) sensorGaugeEl.checked = data.sensor_gauge === '1' || data.sensor_gauge === 1;
    const sensorGaugeMinEl = document.getElementById(prefix + '_sensor_gauge_min');
    if (sensorGaugeMinEl) sensorGaugeMinEl.value = data.sensor_gauge_min || '';
    const sensorGaugeMaxEl = document.getElementById(prefix + '_sensor_gauge_max');
    if (sensorGaugeMaxEl) sensorGaugeMaxEl.value = data.sensor_gauge_max || '';
    syncGaugeUi(tab);

    const sceneEl = document.getElementById(prefix + '_scene_alias');
    if (sceneEl) sceneEl.value = data.scene_alias || '';
    const keyEl = document.getElementById(prefix + '_key_macro');
    if (keyEl) keyEl.value = data.key_macro || '';
    const navEl = document.getElementById(prefix + '_navigate_target');
    if (navEl) navEl.value = data.navigate_target || '0';

    const switchEntityEl = document.getElementById(prefix + '_switch_entity');
    if (switchEntityEl) switchEntityEl.value = data.switch_entity || '';
    const switchStyleEl = document.getElementById(prefix + '_switch_style');
    if (switchStyleEl) switchStyleEl.value = data.switch_style || '0';
    if (typeValue === '6') {
      const imagePathEl = document.getElementById(prefix + '_image_path');
      if (imagePathEl) imagePathEl.value = data.image_path || '';
      const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
      if (intervalEl) intervalEl.value = data.image_slideshow_sec || '10';
      applyImageUiState(tab, data.image_path || '');
      refreshImageSelect(tab, false);
    } else if (typeValue === '9') {
      const flags = (data.clock_flags !== undefined && data.clock_flags !== null)
        ? data.clock_flags
        : data.sensor_decimals;
      applyClockFlagsToInputs(prefix, flags);
    }
  }

  function copyTile(tab) {
    if (currentTileIndex === -1 || currentTileTab !== tab) {
      showNotification('Bitte zuerst eine Kachel waehlen', false);
      return;
    }
    tileClipboard = collectTileFormData(tab);
    persistTileClipboard();
    showNotification('Kachel kopiert');
  }

  function pasteTile(tab) {
    if (currentTileIndex === -1 || currentTileTab !== tab) {
      showNotification('Bitte zuerst eine Kachel waehlen', false);
      return;
    }
    if (!tileClipboard) {
      showNotification('Keine kopierte Kachel vorhanden', false);
      return;
    }
    applyTileFormData(tab, tileClipboard);
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
    showNotification('Kachel eingefuegt');
  }

  function selectTile(index, tab) {
    currentTileIndex = index;
    currentTileTab = tab;
    document.querySelectorAll('.tile').forEach(t => t.classList.remove('active', 'drop-target', 'dragging'));
    const tileId = tab + '-tile-' + index;
    document.getElementById(tileId)?.classList.add('active');
    const settingsId = tab + 'Settings';
    const settingsPanel = document.getElementById(settingsId);
    if (settingsPanel) {
      const tileSpecific = settingsPanel.querySelector('.tile-specific-settings');
      if (tileSpecific) tileSpecific.classList.remove('hidden');
    }
    loadTileData(index, tab);
    setupLivePreview(tab);
  }

  function titleFromOption(option) {
    if (!option) return '';
    const label = String(option.textContent || option.innerText || '').trim();
    if (!label.length) return '';
    const sep = label.indexOf(' - ');
    if (sep > 0) return label.substring(0, sep).trim();
    return label;
  }

  function titleFromEntity(entity) {
    let name = String(entity || '').trim();
    if (!name.length) return '';
    const dot = name.indexOf('.');
    if (dot !== -1) name = name.substring(dot + 1);
    name = name.replace(/[_-]+/g, ' ').trim();
    if (!name.length) return '';
    return name.replace(/\b\w/g, (m) => m.toUpperCase());
  }

  function maybeFillTitleFromEntity(tab, selectSuffix) {
    const prefix = tab;
    const titleInput = document.getElementById(prefix + '_tile_title');
    const selectEl = document.getElementById(prefix + selectSuffix);
    if (!titleInput || !selectEl) return;
    if (titleInput.value && titleInput.value.trim().length) return;
    const opt = selectEl.selectedOptions && selectEl.selectedOptions[0];
    let title = titleFromOption(opt);
    if (!title.length) title = titleFromEntity(selectEl.value);
    if (title.length) titleInput.value = title;
  }

  function setupLivePreview(tab) {
    const prefix = tab;
      const fields = [
        '_tile_title','_tile_color','_tile_col','_tile_row','_tile_span_w','_tile_span_h','_tile_type','_sensor_entity','_sensor_unit',
        '_sensor_decimals','_sensor_value_font','_sensor_gauge','_sensor_gauge_min','_sensor_gauge_max',
        '_scene_alias','_key_macro','_navigate_target','_switch_entity','_switch_style',
        '_image_path','_image_select','_image_slideshow_sec','_image_url',
        '_clock_show_time','_clock_show_date'
      ];
    fields.forEach(id => {
      const el = document.getElementById(prefix + id);
      if (el) el.replaceWith(el.cloneNode(true));
    });

    const titleInput = document.getElementById(prefix + '_tile_title');
    const iconInput = document.getElementById(prefix + '_tile_icon');
    const colorInput = document.getElementById(prefix + '_tile_color');
    const colInput = document.getElementById(prefix + '_tile_col');
    const rowInput = document.getElementById(prefix + '_tile_row');
    const spanWInput = document.getElementById(prefix + '_tile_span_w');
    const spanHInput = document.getElementById(prefix + '_tile_span_h');
    const typeSelect = document.getElementById(prefix + '_tile_type');
      const entitySelect = document.getElementById(prefix + '_sensor_entity');
      const unitInput = document.getElementById(prefix + '_sensor_unit');
      const decimalsInput = document.getElementById(prefix + '_sensor_decimals');
      const valueFontSelect = document.getElementById(prefix + '_sensor_value_font');
      const gaugeCheck = document.getElementById(prefix + '_sensor_gauge');
      const gaugeMinInput = document.getElementById(prefix + '_sensor_gauge_min');
      const gaugeMaxInput = document.getElementById(prefix + '_sensor_gauge_max');
      const sceneInput = document.getElementById(prefix + '_scene_alias');
    const keyInput = document.getElementById(prefix + '_key_macro');
    const navigateSelect = document.getElementById(prefix + '_navigate_target');
    const switchSelect = document.getElementById(prefix + '_switch_entity');
    const switchStyleSelect = document.getElementById(prefix + '_switch_style');
    const imageSelect = document.getElementById(prefix + '_image_select');
    const imageUrlInput = document.getElementById(prefix + '_image_url');
    const imageIntervalInput = document.getElementById(prefix + '_image_slideshow_sec');
    const clockTimeCheck = document.getElementById(prefix + '_clock_show_time');
    const clockDateCheck = document.getElementById(prefix + '_clock_show_date');

    if (titleInput) titleInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (iconInput) iconInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (colorInput) colorInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (colInput) colInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (rowInput) rowInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (spanWInput) spanWInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (spanHInput) spanHInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (typeSelect) typeSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (entitySelect) entitySelect.addEventListener('change', () => { maybeFillTitleFromSensor(tab); updateTilePreview(tab); updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (unitInput) unitInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (decimalsInput) decimalsInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (valueFontSelect) valueFontSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeCheck) gaugeCheck.addEventListener('change', () => { syncGaugeUi(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeMinInput) gaugeMinInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeMaxInput) gaugeMaxInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
    if (sceneInput) sceneInput.addEventListener('input', () => { maybeFillTitleFromScene(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (keyInput) keyInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (navigateSelect) navigateSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (switchSelect) switchSelect.addEventListener('change', () => { maybeFillTitleFromSwitch(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (switchStyleSelect) switchStyleSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (imageSelect) imageSelect.addEventListener('focus', () => { refreshImageSelect(tab, true); });
    if (imageSelect) imageSelect.addEventListener('change', () => {
      const selected = imageSelect.value || '';
      if (selected === imageUrlToken) {
        setImageUrl(tab, imageUrlInput ? imageUrlInput.value : '');
      } else {
        setImagePath(tab, selected);
      }
    });
    if (imageUrlInput) imageUrlInput.addEventListener('input', () => {
      setImageUrl(tab, imageUrlInput.value || '');
    });
    if (imageIntervalInput) imageIntervalInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
    if (clockTimeCheck) clockTimeCheck.addEventListener('change', () => {
      ensureClockSelection(prefix);
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    });
    if (clockDateCheck) clockDateCheck.addEventListener('change', () => {
      ensureClockSelection(prefix);
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    });
  }

  function updateTilePreview(tab) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const tileId = tab + '-tile-' + currentTileIndex;
    const tileElem = document.getElementById(tileId);
    if (!tileElem) return;

    const wasActive = tileElem.classList.contains('active');
    const typeWas = tileElem.classList.contains('sensor')   ? '1' :
                    tileElem.classList.contains('scene')    ? '2' :
                    tileElem.classList.contains('key')      ? '3' :
                    tileElem.classList.contains('navigate') ? '4' :
                    tileElem.classList.contains('switch')   ? '5' :
                    tileElem.classList.contains('image')    ? '6' :
                    tileElem.classList.contains('clock')    ? '9' : '0';
    const title = document.getElementById(prefix + '_tile_title').value;
    const color = document.getElementById(prefix + '_tile_color').value;
    const type = document.getElementById(prefix + '_tile_type').value;
    const iconInput = document.getElementById(prefix + '_tile_icon');
    const switchStyle = document.getElementById(prefix + '_switch_style')?.value || '0';
    const sensorValueFont = document.getElementById(prefix + '_sensor_value_font')?.value || '0';
    const sensorValueClass = getSensorValueFontClass(sensorValueFont);
    let iconName = iconInput ? iconInput.value.trim().toLowerCase() : '';

    // Normalize icon name (remove mdi: or mdi- prefix)
    if (iconName.startsWith('mdi:')) iconName = iconName.substring(4);
    else if (iconName.startsWith('mdi-')) iconName = iconName.substring(4);

    tileElem.className = 'tile';
    tileElem.style.background = '';

    if (type === '0') {
      tileElem.classList.add('empty');
      tileElem.innerHTML = '';
      if (wasActive) tileElem.classList.add('active');
    } else if (type === '1') {
      tileElem.classList.add('sensor');
      tileElem.style.background = color || '#2A2A2A';
    } else if (type === '2') {
      tileElem.classList.add('scene');
      tileElem.style.background = color || '#353535';
    } else if (type === '3') {
      tileElem.classList.add('key');
      tileElem.style.background = color || '#353535';
    } else if (type === '4' || type === '7' || type === '8') {
      tileElem.classList.add('navigate');
      tileElem.style.background = color || '#353535';
    } else if (type === '5') {
      tileElem.classList.add('switch');
      if (switchStyle === '1') tileElem.classList.add('switch-toggle');
      tileElem.style.background = color || '#353535';
    } else if (type === '6') {
      tileElem.classList.add('image');
      tileElem.style.background = color || '#353535';
    } else if (type === '9') {
      tileElem.classList.add('clock');
      tileElem.style.background = color || '#353535';
    }

    let html = '';

    if (type !== '0') {
      // Icon (optional)
      if (iconName) {
        html += '<i class="mdi mdi-' + iconName + ' tile-icon"></i>';
      }

      // Title (nur wenn vorhanden)
      if (title) {
        html += '<div class="tile-title" id="' + tileId + '-title">' + title + '</div>';
      }

      if (type === '1') {
        const entitySelect = document.getElementById(prefix + '_sensor_entity');
        const unitInput = document.getElementById(prefix + '_sensor_unit');
        const entity = entitySelect ? entitySelect.value : '';
        const unit = unitInput ? unitInput.value : '';
        html += '<div class="tile-value ' + sensorValueClass + '" id="' + tileId + '-value">--';
        if (unit) html += '<span class="tile-unit">' + unit + '</span>';
        html += '</div>';
        if (entity) {
          tileElem.innerHTML = html;
          if (wasActive) tileElem.classList.add('active');
          updateSensorValuePreview(tab);
        }
      }

      if (type === '9') {
        const flags = getClockFlagsFromInputs(prefix);
        if (flags & 1) html += '<div class="tile-clock-time">' + getClockPreviewTime() + '</div>';
        if (flags & 2) html += '<div class="tile-clock-date">' + getClockPreviewDate() + '</div>';
      }

      if (type === '5' && switchStyle === '1') {
        html += '<div class="tile-switch" id="' + tileId + '-switch"><div class="tile-switch-knob"></div></div>';
      }
    }

    tileElem.innerHTML = html;
    if (wasActive) tileElem.classList.add('active');
    if (typeWas !== type && wasActive) {
      tileElem.classList.add('active');
      const settingsId = tab + 'Settings';
      document.getElementById(settingsId)?.classList.remove('hidden');
    }
    if (type === '5') updateSwitchValuePreview(tab);
    updateLayoutFromInputs(tab);
}

  function loadTileData(index, tab) {
    const folderId = getFolderIdForTab(tab);
    if (folderId === undefined) return;
    fetch('/api/tiles?folder=' + encodeURIComponent(folderId) + '&index=' + index)
      .then(res => res.json())
      .then(data => {
        if (!tilesData[tab]) tilesData[tab] = [];
        tilesData[tab][index] = data;
        const prefix = tab;
        document.getElementById(prefix + '_tile_type').value = data.type || 0;
        updateTileType(tab);
        document.getElementById(prefix + '_tile_title').value = data.title || '';
        document.getElementById(prefix + '_tile_icon').value = data.icon_name || '';
        document.getElementById(prefix + '_tile_color').value = rgbToHex(data.bg_color || 0x2A2A2A);
        const colEl = document.getElementById(prefix + '_tile_col');
        const rowEl = document.getElementById(prefix + '_tile_row');
        const spanWEl = document.getElementById(prefix + '_tile_span_w');
        const spanHEl = document.getElementById(prefix + '_tile_span_h');
        if (colEl && rowEl && spanWEl && spanHEl) {
          const fallbackLayout = (data.type === 0) ? getTileElementLayout(tab, index) : null;
          const layoutInput = {
            col: data.col,
            row: data.row,
            span_w: data.span_w,
            span_h: data.span_h
          };
          if (fallbackLayout) {
            layoutInput.col = fallbackLayout.col;
            layoutInput.row = fallbackLayout.row;
            layoutInput.span_w = fallbackLayout.span_w;
            layoutInput.span_h = fallbackLayout.span_h;
          }
          const layout = normalizeTileLayout(layoutInput, index);
          colEl.value = String(layout.col + 1);
          rowEl.value = String(layout.row + 1);
          spanWEl.value = String(layout.span_w);
          spanHEl.value = String(layout.span_h);
        }
      if (data.type === 1) {
        document.getElementById(prefix + '_sensor_entity').value = data.sensor_entity || '';
        document.getElementById(prefix + '_sensor_unit').value = data.sensor_unit || '';
        const decEl = document.getElementById(prefix + '_sensor_decimals');
        if (decEl) decEl.value = (data.sensor_decimals !== undefined && data.sensor_decimals >= 0) ? data.sensor_decimals : '';
        const fontEl = document.getElementById(prefix + '_sensor_value_font');
        if (fontEl) fontEl.value = (data.sensor_value_font !== undefined) ? String(data.sensor_value_font) : '0';
        const gaugeEl = document.getElementById(prefix + '_sensor_gauge');
        if (gaugeEl) gaugeEl.checked = data.sensor_gauge === 1 || data.sensor_gauge === '1';
        const gaugeMinEl = document.getElementById(prefix + '_sensor_gauge_min');
        if (gaugeMinEl) gaugeMinEl.value = (data.sensor_gauge_min !== undefined && data.sensor_gauge_min !== null) ? String(data.sensor_gauge_min) : '';
        const gaugeMaxEl = document.getElementById(prefix + '_sensor_gauge_max');
        if (gaugeMaxEl) gaugeMaxEl.value = (data.sensor_gauge_max !== undefined && data.sensor_gauge_max !== null) ? String(data.sensor_gauge_max) : '';
        syncGaugeUi(tab);
      } else if (data.type === 2) {
          document.getElementById(prefix + '_scene_alias').value = data.scene_alias || '';
          maybeFillTitleFromScene(tab);
        } else if (data.type === 3) {
          document.getElementById(prefix + '_key_macro').value = data.key_macro || '';
        } else if (data.type === 4) {
          const navEl = document.getElementById(prefix + '_navigate_target');
          if (navEl) navEl.value = (data.navigate_target !== undefined && data.navigate_target !== null) ? String(data.navigate_target) : '0';
        } else if (data.type === 5) {
          document.getElementById(prefix + '_switch_entity').value = data.sensor_entity || '';
          const styleEl = document.getElementById(prefix + '_switch_style');
          if (styleEl) styleEl.value = (data.switch_style !== undefined) ? String(data.switch_style) : '0';
        } else if (data.type === 6) {
          document.getElementById(prefix + '_image_path').value = data.image_path || '';
          const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
          if (intervalEl) intervalEl.value = data.image_slideshow_sec || '10';
          applyImageUiState(tab, data.image_path || '');
          refreshImageSelect(tab, false);
        } else if (data.type === 9) {
          applyClockFlagsToInputs(prefix, data.sensor_decimals);
        }
      const decEl = document.getElementById(prefix + '_sensor_decimals');
      if (data.type !== 1 && decEl) decEl.value = '';
      const fontEl = document.getElementById(prefix + '_sensor_value_font');
      if (data.type !== 1 && fontEl) fontEl.value = '0';
      const gaugeEl = document.getElementById(prefix + '_sensor_gauge');
      if (data.type !== 1 && gaugeEl) gaugeEl.checked = false;
        const gaugeMinEl = document.getElementById(prefix + '_sensor_gauge_min');
        if (data.type !== 1 && gaugeMinEl) gaugeMinEl.value = '';
        const gaugeMaxEl = document.getElementById(prefix + '_sensor_gauge_max');
        if (data.type !== 1 && gaugeMaxEl) gaugeMaxEl.value = '';
        syncGaugeUi(tab);
        const tileElem = document.getElementById(tab + '-tile-' + index);
        if (tileElem) tileElem.classList.add('active');
        const draft = (drafts[tab] || {})[index];
        if (draft && String(draft.type) === String(data.type)) {
          applyDraft(tab, index);
        } else {
          if (draft && data.type === 0 && draft.type !== data.type) clearDraft(tab, index);
          updateTilePreview(tab);
        }
    });
  }

  function getCurrentTileType(tab) {
    const tiles = getTilesData(tab);
    if (tiles && currentTileIndex >= 0 && tiles[currentTileIndex]) {
      return String(tiles[currentTileIndex].type ?? '0');
    }
    const typeEl = document.getElementById(tab + '_tile_type');
    return typeEl ? String(typeEl.value) : '0';
  }

  function isLockedTileType(typeValue) {
    return String(typeValue) === '7' || String(typeValue) === '8';
  }

  function applySpecialTileUiState(tab) {
    const prefix = tab;
    const typeEl = document.getElementById(prefix + '_tile_type');
    const navSelect = document.getElementById(prefix + '_navigate_target');
    const noteEl = document.getElementById(prefix + '_navigate_note');
    const typeValue = typeEl ? String(typeEl.value) : '0';
    const locked = isLockedTileType(typeValue);
    if (typeEl) typeEl.disabled = locked;
    if (navSelect) navSelect.disabled = (typeValue !== '4');
    if (noteEl) {
      if (typeValue === '7') noteEl.textContent = 'Settings-Kachel (fest)';
      else if (typeValue === '8') noteEl.textContent = 'Zurueck-Kachel (fest)';
      else noteEl.textContent = '';
    }
  }

  function updateTileType(tab) {
    const prefix = tab;
    const typeEl = document.getElementById(prefix + '_tile_type');
    let typeValue = typeEl ? typeEl.value : '0';
    document.querySelectorAll('#' + prefix + 'Settings .type-fields').forEach(f => f.classList.remove('show'));
    if (typeValue === '1') document.getElementById(prefix + '_sensor_fields').classList.add('show');
    else if (typeValue === '2') document.getElementById(prefix + '_scene_fields').classList.add('show');
    else if (typeValue === '3') document.getElementById(prefix + '_key_fields').classList.add('show');
    else if (typeValue === '4') document.getElementById(prefix + '_navigate_fields').classList.add('show');
    else if (typeValue === '5') document.getElementById(prefix + '_switch_fields').classList.add('show');
    else if (typeValue === '6') {
      document.getElementById(prefix + '_image_fields').classList.add('show');
      refreshImageSelect(tab, false);
      const path = document.getElementById(prefix + '_image_path')?.value || '';
      applyImageUiState(tab, path);
    } else if (typeValue === '9') {
      const clockFields = document.getElementById(prefix + '_clock_fields');
      if (clockFields) clockFields.classList.add('show');
    }
    syncGaugeUi(tab);
    applySpecialTileUiState(tab);
  }

  function showNotification(message, success = true) {
    const notification = document.getElementById('notification');
    notification.textContent = message;
    notification.style.background = success ? '#10b981' : '#ef4444';
    notification.classList.add('show');
    setTimeout(() => { notification.classList.remove('show'); }, 3000);
  }

  function scheduleAutoSave(tab) {
    if (autoSaveTimers[tab]) clearTimeout(autoSaveTimers[tab]);
    autoSaveTimers[tab] = setTimeout(() => saveTile(tab, true), 250);
  }

  function resetTile(tab) {
    if (currentTileIndex === -1) return;
    const tileType = getCurrentTileType(tab);
    if (isLockedTileType(tileType)) {
      showNotification('Diese Kachel kann nicht geloescht werden', false);
      return;
    }
    const prefix = tab;
    document.getElementById(prefix + '_tile_type').value = '0';
    document.getElementById(prefix + '_tile_title').value = '';
    document.getElementById(prefix + '_tile_icon').value = '';
    document.getElementById(prefix + '_tile_color').value = '#2A2A2A';
    ['_sensor_entity','_sensor_unit','_sensor_decimals','_sensor_value_font','_scene_alias','_key_macro','_switch_entity','_switch_style','_image_path'].forEach(suf => {
      const el = document.getElementById(prefix + suf);
      if (el) el.value = (suf === '_switch_style' || suf === '_sensor_value_font') ? '0' : '';
    });
    const gaugeEl = document.getElementById(prefix + '_sensor_gauge');
    if (gaugeEl) gaugeEl.checked = false;
    const gaugeMinEl = document.getElementById(prefix + '_sensor_gauge_min');
    if (gaugeMinEl) gaugeMinEl.value = '';
    const gaugeMaxEl = document.getElementById(prefix + '_sensor_gauge_max');
    if (gaugeMaxEl) gaugeMaxEl.value = '';
    const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
    if (intervalEl) intervalEl.value = '10';
    const urlEl = document.getElementById(prefix + '_image_url');
    if (urlEl) urlEl.value = '';
    updateImageUrlVisibility(tab, '', '');
    applyImageUiState(tab, '');
    applyClockFlagsToInputs(prefix, 1);
    syncGaugeUi(tab);
    updateTileType(tab);
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
  }

  function saveTile(tab, silent = false) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const tiles = getTilesData(tab);
    const previousTile = Array.isArray(tiles) ? tiles[currentTileIndex] : null;
    const previousType = previousTile ? Number(previousTile.type) : NaN;
    const formData = new FormData();
    const layout = normalizeLayoutInputs(tab);
    const folderId = getFolderIdForTab(tab);
    if (folderId === undefined) {
      showNotification('Ordner nicht gefunden', false);
      return;
    }
    formData.append('folder', folderId);
    formData.append('index', currentTileIndex);
    formData.append('col', layout.col);
    formData.append('row', layout.row);
    formData.append('span_w', layout.span_w);
    formData.append('span_h', layout.span_h);
    formData.append('type', document.getElementById(prefix + '_tile_type').value);
    formData.append('title', document.getElementById(prefix + '_tile_title').value);
    formData.append('icon_name', document.getElementById(prefix + '_tile_icon').value);
    formData.append('bg_color', hexToRgb(document.getElementById(prefix + '_tile_color').value));
    const typeValue = document.getElementById(prefix + '_tile_type').value;
      if (typeValue === '1') {
        formData.append('sensor_entity', document.getElementById(prefix + '_sensor_entity').value);
        formData.append('sensor_unit', document.getElementById(prefix + '_sensor_unit').value);
        formData.append('sensor_decimals', document.getElementById(prefix + '_sensor_decimals').value);
        formData.append('sensor_value_font', document.getElementById(prefix + '_sensor_value_font').value);
        formData.append('sensor_gauge', document.getElementById(prefix + '_sensor_gauge')?.checked ? '1' : '0');
        formData.append('sensor_gauge_min', document.getElementById(prefix + '_sensor_gauge_min')?.value || '');
        formData.append('sensor_gauge_max', document.getElementById(prefix + '_sensor_gauge_max')?.value || '');
      } else if (typeValue === '2') {
      formData.append('scene_alias', document.getElementById(prefix + '_scene_alias').value);
    } else if (typeValue === '3') {
      formData.append('key_macro', document.getElementById(prefix + '_key_macro').value);
      } else if (typeValue === '4') {
        const navTargetElement = document.getElementById(prefix + '_navigate_target');
        const navTargetValue = navTargetElement ? navTargetElement.value : '0';
        formData.append('navigate_target', navTargetValue);
        } else if (typeValue === '5') {
        formData.append('switch_entity', document.getElementById(prefix + '_switch_entity').value);
        const styleEl = document.getElementById(prefix + '_switch_style');
        formData.append('switch_style', styleEl ? styleEl.value : '0');
      } else if (typeValue === '6') {
        const imgSelect = document.getElementById(prefix + '_image_select');
        const imgUrl = document.getElementById(prefix + '_image_url');
        const imgPath = document.getElementById(prefix + '_image_path');
        let finalPath = imgPath ? imgPath.value : '';
        if (imgSelect && imgSelect.value === imageUrlToken && imgUrl && imgUrl.value.trim().length > 0) {
          finalPath = imgUrl.value.trim();
          if (imgPath) imgPath.value = finalPath;
        }
        formData.append('image_path', finalPath);
        formData.append('image_slideshow_sec', document.getElementById(prefix + '_image_slideshow_sec').value);
      } else if (typeValue === '9') {
        ensureClockSelection(prefix);
        const flags = getClockFlagsFromInputs(prefix);
        formData.append('clock_show_time', (flags & 1) ? '1' : '0');
        formData.append('clock_show_date', (flags & 2) ? '1' : '0');
      }
      fetch('/api/tiles', { method:'POST', body:formData })
        .then(res => res.json())
        .then(data => {
          if (data.success) {
            if (!silent) showNotification('Kachel gespeichert & Display aktualisiert!');
            clearDraft(tab, currentTileIndex);
            loadSensorValues();
            if (typeValue === '4') {
              const navTarget = document.getElementById(prefix + '_navigate_target')?.value || '';
              if (String(navTarget) === '0') {
                setTimeout(() => location.reload(), 400);
              } else {
                const titleVal = document.getElementById(prefix + '_tile_title')?.value || '';
                const iconVal = document.getElementById(prefix + '_tile_icon')?.value || '';
                updateFolderTabUi(navTarget, titleVal, iconVal);
              }
            }
            if (previousType === 4 && typeValue === '0') {
              setTimeout(() => location.reload(), 400);
            }
          } else {
            showNotification('Fehler: ' + (data.error || 'Unbekannt'), false);
          }
        })
      .catch(() => showNotification('Netzwerkfehler beim Speichern', false));
  }

  function downloadJsonFile(filename, content) {
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 5000);
  }

  function parseBgColorValue(value) {
    if (value === undefined || value === null) return 0;
    if (typeof value === 'string') {
      let v = value.trim();
      if (!v.length) return 0;
      if (v.startsWith('#')) return parseInt(v.substring(1), 16) || 0;
      if (v.startsWith('0x') || v.startsWith('0X')) return parseInt(v, 16) || 0;
    }
    const num = parseInt(value, 10);
    return isNaN(num) ? 0 : num;
  }

  async function exportTilesConfig() {
    try {
      const tabs = tileTabs.slice();
      const tileRequests = tabs.map(tab => {
        const folderId = getFolderIdForTab(tab);
        if (folderId === undefined) return Promise.resolve([]);
        return fetch('/api/tiles?folder=' + encodeURIComponent(folderId))
          .then(res => res.json())
          .catch(() => []);
      });
      const tilesLists = await Promise.all(tileRequests);
      const folders = tabs.map(tab => {
        const folderId = getFolderIdForTab(tab);
        const tabEl = document.getElementById('tab-tiles-' + tab);
        const parentId = tabEl ? parseInt(tabEl.dataset.folderParent || '0', 10) : 0;
        return {
          id: folderId,
          parent_id: isNaN(parentId) ? 0 : parentId,
          name: tabEl ? (tabEl.dataset.folderName || '') : '',
          icon_name: tabEl ? (tabEl.dataset.folderIcon || '') : ''
        };
      }).filter(entry => entry.id !== undefined);

      const grids = {};
      tabs.forEach((tab, idx) => {
        const folderId = getFolderIdForTab(tab);
        if (folderId === undefined) return;
        grids[String(folderId)] = Array.isArray(tilesLists[idx]) ? tilesLists[idx] : [];
      });

      const payload = {
        version: 2,
        exported_at: new Date().toISOString(),
        folders: folders,
        grids: grids
      };
      const ts = new Date().toISOString().replace(/[:.]/g, '-');
      downloadJsonFile('tab5_tiles_' + ts + '.json', JSON.stringify(payload, null, 2));
      showNotification('Export erstellt!');
    } catch (e) {
      showNotification('Export fehlgeschlagen', false);
    }
  }

  function triggerTilesImport(tab) {
    const input = document.getElementById(tab + '_tile_import');
    if (!input) return;
    input.value = '';
    input.click();
  }

  function importTilesConfig(tab, files) {
    if (!files || !files.length) return;
    const file = files[0];
    const reader = new FileReader();
    reader.onload = async () => {
      try {
        const payload = JSON.parse(reader.result);
        await importTilesPayload(payload);
      } catch (e) {
        showNotification('Import-JSON ungueltig', false);
      }
    };
    reader.onerror = () => showNotification('Import fehlgeschlagen', false);
    reader.readAsText(file);
  }

  async function importTilesPayload(payload) {
    try {
      if (!payload || typeof payload !== 'object') {
        showNotification('Import-JSON ungueltig', false);
        return;
      }
      const grids = (payload.grids && typeof payload.grids === 'object') ? payload.grids : {};
      if (!Object.keys(grids).length) {
        if (Array.isArray(payload.tab0)) grids['0'] = payload.tab0;
        if (Array.isArray(payload.tab1)) grids['1'] = payload.tab1;
        if (Array.isArray(payload.tab2)) grids['2'] = payload.tab2;
      }

      showNotification('Import laeuft...');

      const entries = Object.entries(grids);
      for (const [folderIdRaw, tiles] of entries) {
        const folderId = parseInt(folderIdRaw, 10);
        if (isNaN(folderId)) continue;
        if (!Array.isArray(tiles)) continue;
        if (!tabByFolder[folderId]) continue;
        for (let i = 0; i < tiles.length && i < (GRID_COLS * GRID_ROWS); i++) {
          await postTile(folderId, i, tiles[i] || {});
        }
      }

      try { localStorage.removeItem('tileDrafts'); } catch (e) {}
      showNotification('Import abgeschlossen!');
      setTimeout(() => location.reload(), 600);
    } catch (e) {
      console.error('Import fehlgeschlagen:', e);
      showNotification('Import fehlgeschlagen', false);
    }
  }

  async function postTile(folderId, index, tile) {
    const fd = new FormData();
    const type = Number(tile.type);
    let safeType = isNaN(type) ? 0 : type;
    if (safeType === 4 && tile.navigate_kind !== undefined && tile.navigate_kind !== null) {
      const kind = Number(tile.navigate_kind);
      if (kind === 1) safeType = 7;
      else if (kind === 2) safeType = 8;
    }
    fd.append('folder', folderId);
    fd.append('index', index);
    fd.append('type', safeType);
    fd.append('title', tile.title || '');
    fd.append('icon_name', tile.icon_name || '');
    fd.append('bg_color', parseBgColorValue(tile.bg_color));
    const layout = normalizeTileLayout(tile, index);
    fd.append('col', layout.col);
    fd.append('row', layout.row);
    fd.append('span_w', layout.span_w);
    fd.append('span_h', layout.span_h);

    if (safeType === 1) {
      fd.append('sensor_entity', tile.sensor_entity || '');
      fd.append('sensor_unit', tile.sensor_unit || '');
      const dec = tile.sensor_decimals;
      if (dec !== undefined && dec !== null && Number(dec) >= 0) {
        fd.append('sensor_decimals', dec);
      }
      if (tile.sensor_value_font !== undefined && tile.sensor_value_font !== null) {
        fd.append('sensor_value_font', tile.sensor_value_font);
      }
      fd.append('sensor_gauge', (tile.sensor_gauge === 1 || tile.sensor_gauge === '1' || tile.sensor_gauge === true) ? '1' : '0');
      if (tile.sensor_gauge_min !== undefined && tile.sensor_gauge_min !== null && String(tile.sensor_gauge_min).length > 0) {
        fd.append('sensor_gauge_min', tile.sensor_gauge_min);
      }
      if (tile.sensor_gauge_max !== undefined && tile.sensor_gauge_max !== null && String(tile.sensor_gauge_max).length > 0) {
        fd.append('sensor_gauge_max', tile.sensor_gauge_max);
      }
    } else if (safeType === 2) {
      fd.append('scene_alias', tile.scene_alias || '');
    } else if (safeType === 3) {
      fd.append('key_macro', tile.key_macro || '');
    } else if (safeType === 4) {
      const target = (tile.navigate_target !== undefined && tile.navigate_target !== null)
        ? tile.navigate_target
        : 0;
      fd.append('navigate_target', target);
    } else if (safeType === 5) {
      fd.append('switch_entity', tile.sensor_entity || '');
      const style = (tile.switch_style !== undefined && tile.switch_style !== null)
        ? tile.switch_style
        : (tile.sensor_decimals === 1 ? 1 : 0);
      fd.append('switch_style', style);
    } else if (safeType === 6) {
      fd.append('image_path', tile.image_path || '');
      fd.append('image_slideshow_sec', tile.image_slideshow_sec || '10');
    }

    const res = await fetch('/api/tiles', { method: 'POST', body: fd });
    const data = await res.json();
    if (!data.success) {
      throw new Error('Tile speichern fehlgeschlagen');
    }
  }

  function rgbToHex(rgb) { return '#' + ('000000' + rgb.toString(16)).slice(-6); }
  function hexToRgb(hex) { return parseInt(hex.replace('#', ''), 16); }

  function renderTileFromData(tab, index, tile, sensorValues) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return;
    el.dataset.index = index.toString();
    let cls = ['tile'];
    if (tile.type === 1) cls.push('sensor');
    else if (tile.type === 2) cls.push('scene');
    else if (tile.type === 3) cls.push('key');
    else if (tile.type === 4 || tile.type === 7 || tile.type === 8) cls.push('navigate');
    else if (tile.type === 5) {
      cls.push('switch');
      if (tile.switch_style === 1) cls.push('switch-toggle');
    } else if (tile.type === 6) cls.push('image');
    else if (tile.type === 9) cls.push('clock');
    else cls.push('empty');
    el.className = cls.join(' ');
    if (tile.type === 0) el.style.background = 'transparent';
    else if (tile.type === 1) el.style.background = tile.bg_color ? ('#' + ('000000' + tile.bg_color.toString(16)).slice(-6)) : '#2A2A2A';
    else el.style.background = tile.bg_color ? ('#' + ('000000' + tile.bg_color.toString(16)).slice(-6)) : '#353535';
      const sensorValueClass = getSensorValueFontClass(tile.sensor_value_font);
      if (tile.type === 0) { el.innerHTML = ''; }
      else {
      // Icon (optional) - normalize icon name
      let iconName = (tile.icon_name || '').trim().toLowerCase();
      if (iconName.startsWith('mdi:')) iconName = iconName.substring(4);
      else if (iconName.startsWith('mdi-')) iconName = iconName.substring(4);

      let html = '';

      // Icon (if exists)
      if (iconName) {
        html += '<i class="mdi mdi-' + iconName + ' tile-icon"></i>';
      }

      // Title (nur wenn vorhanden)
      if (tile.title && tile.title.length) {
        html += '<div class="tile-title" id="' + tab + '-tile-' + index + '-title">' + tile.title + '</div>';
      }

      // Sensor value
      if (tile.type === 1) {
        let value = '--';
        if (tile.sensor_entity) value = formatSensorValue(sensorValues[tile.sensor_entity] ?? '--', tile.sensor_decimals);
        const unit = tile.sensor_unit || '';
        html += '<div class="tile-value ' + sensorValueClass + '" id="' + tab + '-tile-' + index + '-value">' + value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '') + '</div>';
      }
      if (tile.type === 9) {
        const flags = normalizeClockFlags(tile.sensor_decimals);
        if (flags & 1) html += '<div class="tile-clock-time">' + getClockPreviewTime() + '</div>';
        if (flags & 2) html += '<div class="tile-clock-date">' + getClockPreviewDate() + '</div>';
      }
      if (tile.type === 5 && tile.switch_style === 1) {
        html += '<div class="tile-switch" id="' + tab + '-tile-' + index + '-switch"><div class="tile-switch-knob"></div></div>';
      }
      el.innerHTML = html;
    }
    if (currentTileTab === tab && currentTileIndex === index) el.classList.add('active');
    if (tile.type === 5 && tile.sensor_entity) {
      const state = parseSwitchPayload(sensorValues[tile.sensor_entity] ?? '');
      applySwitchPreviewState(el, state);
    }
  }

  function loadSensorValues() {
    const tabs = tileTabs.slice();
    const tileRequests = tabs.map(tab => {
      const folderId = getFolderIdForTab(tab);
      if (folderId === undefined) return Promise.resolve([]);
      return fetch('/api/tiles?folder=' + encodeURIComponent(folderId))
        .then(res => res.json())
        .catch(() => []);
    });

    Promise.all([
      fetch('/api/sensor_values').then(res => res.json()).catch(() => ({})),
      ...tileRequests
    ])
    .then(results => {
      const sensorValues = results[0] || {};
      tabs.forEach((tab, idx) => {
        const tiles = Array.isArray(results[idx + 1]) ? results[idx + 1] : [];
        tilesData[tab] = tiles;
        tiles.forEach((tile, i) => renderTileFromData(tab, i, tile, sensorValues));
        layoutTiles(tab, tiles);
      });
      if (currentTileIndex !== -1 && currentTileTab) {
        const settingsId = currentTileTab + 'Settings';
        document.getElementById(settingsId)?.classList.remove('hidden');
        const activeTile = document.getElementById(currentTileTab + '-tile-' + currentTileIndex);
        if (activeTile) activeTile.classList.add('active');
      }
    })
    .catch(err => console.error('Fehler beim Laden der Sensorwerte:', err));
  }

  let dragSource = null;
  let dragPreview = null;

  function createDragPreview(tile) {
    const clone = tile.cloneNode(true);
    const rect = tile.getBoundingClientRect();
    clone.style.position = 'absolute';
    clone.style.top = '-9999px';
    clone.style.left = '-9999px';
    clone.style.width = rect.width + 'px';
    clone.style.height = rect.height + 'px';
    clone.style.opacity = '0.9';
    clone.style.pointerEvents = 'none';
    clone.style.boxShadow = '0 10px 30px rgba(0,0,0,0.35)';
    clone.style.backgroundClip = 'padding-box';
    clone.style.clipPath = 'inset(0 round 11px)';
    clone.style.display = 'block';
    document.body.appendChild(clone);
    return clone;
  }

  function enableTileDrag(tab) {
    const tiles = document.querySelectorAll('#tab-tiles-' + tab + ' .tile');
    tiles.forEach(tile => {
      tile.addEventListener('dragstart', (e) => {
        dragSource = { tab, index: parseInt(tile.dataset.index) };
        e.dataTransfer.effectAllowed = 'move';
        if (!isNaN(dragSource.index)) selectTile(dragSource.index, tab);
        tile.classList.add('dragging');
        if (e.dataTransfer.setDragImage) {
          dragPreview = createDragPreview(tile);
          e.dataTransfer.setDragImage(dragPreview, tile.clientWidth / 2, tile.clientHeight / 2);
        }
      });
      tile.addEventListener('dragend', () => {
        tile.classList.remove('dragging');
        tiles.forEach(t => t.classList.remove('drop-target'));
        if (dragPreview && dragPreview.parentNode) dragPreview.parentNode.removeChild(dragPreview);
        dragPreview = null;
        dragSource = null;
      });
      tile.addEventListener('dragenter', (e) => { e.preventDefault(); tile.classList.add('drop-target'); });
      tile.addEventListener('dragover', (e) => { e.preventDefault(); tile.classList.add('drop-target'); });
      tile.addEventListener('dragleave', () => { tile.classList.remove('drop-target'); });
      tile.addEventListener('drop', (e) => {
        e.preventDefault();
        tile.classList.remove('drop-target');
        if (!dragSource) return;
        const targetIndex = parseInt(tile.dataset.index);
        if (isNaN(targetIndex)) return;
        if (dragSource.index === targetIndex) return;
        reorderTiles(dragSource.tab, dragSource.index, targetIndex, tile.dataset.col, tile.dataset.row);
      });
    });
  }

  function reorderTiles(tab, fromIdx, toIdx, targetCol, targetRow) {
    const sourceLayout = getTileElementLayout(tab, fromIdx);
    const targetLayout = getTileElementLayout(tab, toIdx);
    let col = parseInt(targetCol, 10);
    let row = parseInt(targetRow, 10);
    if (isNaN(col)) col = -1;
    if (isNaN(row)) row = -1;
    const folderId = getFolderIdForTab(tab);
    if (folderId === undefined) {
      showNotification('Ordner nicht gefunden', false);
      return;
    }
    fetch('/api/tiles/reorder', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'folder=' + encodeURIComponent(folderId) +
            '&from=' + encodeURIComponent(fromIdx) +
            '&to=' + encodeURIComponent(toIdx) +
            '&target_col=' + encodeURIComponent(col) +
            '&target_row=' + encodeURIComponent(row)
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
          showNotification('Kacheln verschoben & gespeichert!');
          const tiles = getTilesData(tab);
          if (tiles[fromIdx]) {
            tiles[fromIdx].col = (col >= 0) ? col : (targetLayout ? targetLayout.col : tiles[fromIdx].col);
            tiles[fromIdx].row = (row >= 0) ? row : (targetLayout ? targetLayout.row : tiles[fromIdx].row);
          }
          if (toIdx !== fromIdx && tiles[toIdx] && sourceLayout) {
            tiles[toIdx].col = sourceLayout.col;
            tiles[toIdx].row = sourceLayout.row;
          }
          tilesData[tab] = tiles;
          if (currentTileTab === tab && currentTileIndex !== -1) {
            if (currentTileIndex === fromIdx) applyLayoutInputsFromLayout(tab, targetLayout);
            else if (currentTileIndex === toIdx) applyLayoutInputsFromLayout(tab, sourceLayout);
          }
          loadSensorValues();
      } else {
        showNotification('Fehler beim Verschieben', false);
      }
    })
    .catch(() => showNotification('Netzwerkfehler beim Verschieben', false));
  }

  function loadTileDataAndSelect(tab, index) { selectTile(index, tab); }

  document.addEventListener('DOMContentLoaded', () => {
    initTileTabs();
    loadDraftsFromStorage();
    loadTileClipboard();
    loadSensorValues();
    let savedTab = null;
    try { savedTab = localStorage.getItem('activeAdminTab'); } catch (e) {}
    const defaultTab = tileTabs.length ? ('tab-tiles-' + tileTabs[0]) : 'tab-network';
    const targetTab = savedTab && document.getElementById(savedTab) ? savedTab : defaultTab;
    const targetBtn = Array.from(document.querySelectorAll('.tab-btn')).find(btn => btn.getAttribute('onclick')?.includes(targetTab)) || document.querySelector('.tab-btn');
    if (targetBtn) targetBtn.click();
    setInterval(loadSensorValues, 5000);
    tileTabs.forEach(tab => enableTileDrag(tab));
  });
  </script>
)html";

  append_sensor_scripts(html);
  append_scene_scripts(html);
  append_key_scripts(html);
  append_switch_scripts(html);
  append_navigate_scripts(html);
  append_image_scripts(html);
  append_clock_scripts(html);
}



