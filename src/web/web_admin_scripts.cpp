#include "src/web/web_admin_scripts.h"
#include "src/types/types_registry.h"

void appendAdminScripts(String& html) {
  html += R"html(
  <script>
)html";
  append_tile_type_registry_js(html);
  html += R"html(
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
  let sensorMetaCache = { values: {}, units: {}, icons: {}, names: {}, loaded: false };

  function normalizeSensorMetaPayload(payload) {
    if (!payload || typeof payload !== 'object') {
      return { values: {}, units: {}, icons: {}, names: {}, loaded: false };
    }
    const hasMeta = Object.prototype.hasOwnProperty.call(payload, 'values') ||
                    Object.prototype.hasOwnProperty.call(payload, 'units') ||
                    Object.prototype.hasOwnProperty.call(payload, 'icons') ||
                    Object.prototype.hasOwnProperty.call(payload, 'names');
    if (!hasMeta) {
      return { values: payload || {}, units: {}, icons: {}, names: {}, loaded: true };
    }
    return {
      values: payload.values || {},
      units: payload.units || {},
      icons: payload.icons || {},
      names: payload.names || {},
      loaded: true
    };
  }

  function isSensorMetaCacheLoaded() {
    return !!(sensorMetaCache && sensorMetaCache.loaded);
  }

  function fetchSensorMetaCache() {
    return fetch('/api/sensor_values')
      .then(res => res.json())
      .then(raw => {
        sensorMetaCache = normalizeSensorMetaPayload(raw || {});
        return sensorMetaCache;
      })
      .catch(() => sensorMetaCache);
  }

  function isExplicitlyDisabledValue(raw) {
    if (raw === undefined || raw === null) return false;
    const text = String(raw);
    if (!text.length) return false;
    const trimmed = text.trim().toLowerCase();
    if (!trimmed.length) return true;
    return trimmed === '-' || trimmed === 'none' || trimmed === 'null' || trimmed === 'no' || trimmed === 'off';
  }

  function normalizeMdiIconName(raw) {
    let iconName = String(raw || '').trim().toLowerCase();
    if (iconName.startsWith('mdi:')) iconName = iconName.substring(4);
    else if (iconName.startsWith('mdi-')) iconName = iconName.substring(4);
    return iconName;
  }

  function resolveIconName(rawIcon, entityId, metaIcons) {
    if (isExplicitlyDisabledValue(rawIcon)) return '';
    const direct = normalizeMdiIconName(rawIcon);
    if (direct) return direct;
    if (entityId && metaIcons && metaIcons[entityId]) {
      return normalizeMdiIconName(metaIcons[entityId]);
    }
    return '';
  }

  function resolveUnitValue(rawUnit, entityId, metaUnits) {
    if (isExplicitlyDisabledValue(rawUnit)) return '';
    const direct = String(rawUnit || '').trim();
    if (direct.length) return rawUnit;
    if (entityId && metaUnits && metaUnits[entityId]) {
      return metaUnits[entityId];
    }
    return '';
  }

  function getTileTypeMeta(typeValue) {
    const key = String(typeValue ?? '0');
    return TILE_TYPE_REGISTRY[key] || TILE_TYPE_REGISTRY['0'] || {};
  }

  function callTypeHandler(meta, handlerKey, ...args) {
    if (!meta || !handlerKey) return;
    const fnName = meta[handlerKey];
    if (!fnName) return;
    const fn = window[fnName];
    if (typeof fn === 'function') return fn(...args);
  }

  function collectTypeFieldValues(tab) {
    const prefix = tab;
    const typeValue = document.getElementById(prefix + '_tile_type')?.value || '0';
    const meta = getTileTypeMeta(typeValue);
    if (!meta.save) return {};
    const fd = new FormData();
    callTypeHandler(meta, 'save', prefix, fd);
    const out = {};
    for (const [key, value] of fd.entries()) {
      out[key] = value;
    }
    return out;
  }

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
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
    Object.assign(d, collectTypeFieldValues(tab));
    d._dirty = true;
    drafts[tab][currentTileIndex] = d;
    persistDrafts();
  }

  function applyDraft(tab, index) {
    const d = drafts[tab] && drafts[tab][index];
    if (!d || !d._dirty) return false;
    const prefix = tab;
    document.getElementById(prefix + '_tile_type').value = d.type || '0';
    resetAllTypeFields(tab);
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
    const meta = getTileTypeMeta(d.type || '0');
    callTypeHandler(meta, 'load', prefix, d);
    syncGaugeUi(tab);
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
    const data = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: document.getElementById(prefix + '_tile_color')?.value || '#2A2A2A',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
    Object.assign(data, collectTypeFieldValues(tab));
    return data;
  }

  function applyTileFormData(tab, data) {
    if (!data) return;
    const prefix = tab;
    const typeValue = data.type || '0';
    const typeEl = document.getElementById(prefix + '_tile_type');
    if (typeEl) typeEl.value = typeValue;
    resetAllTypeFields(tab);
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
    const meta = getTileTypeMeta(typeValue);
    callTypeHandler(meta, 'load', prefix, data);
    syncGaugeUi(tab);
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
        '_sensor_decimals','_sensor_value_font','_sensor_display_mode','_sensor_gauge_min','_sensor_gauge_max',
        '_sensor_gauge_arc','_sensor_gauge_size','_sensor_gauge_y_offset','_sensor_value_y_offset','_sensor_graph_height',
        '_weather_entity',
        '_scene_alias','_scene_image_path','_scene_icon_image',
        '_key_macro','_text_value','_text_value_font','_navigate_target','_switch_entity','_switch_style',
        '_image_path','_image_select','_image_slideshow_sec','_image_url','_image_preview',
        '_clock_show_time','_clock_show_date',
        '_counter_value'
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
      const displayModeSelect = document.getElementById(prefix + '_sensor_display_mode');
      const gaugeMinInput = document.getElementById(prefix + '_sensor_gauge_min');
      const gaugeMaxInput = document.getElementById(prefix + '_sensor_gauge_max');
      const gaugeArcInput = document.getElementById(prefix + '_sensor_gauge_arc');
      const gaugeSizeInput = document.getElementById(prefix + '_sensor_gauge_size');
      const gaugeYOffsetInput = document.getElementById(prefix + '_sensor_gauge_y_offset');
      const valueYOffsetInput = document.getElementById(prefix + '_sensor_value_y_offset');
      const graphHeightInput = document.getElementById(prefix + '_sensor_graph_height');
      const weatherSelect = document.getElementById(prefix + '_weather_entity');
      const sceneInput = document.getElementById(prefix + '_scene_alias');
    const keyInput = document.getElementById(prefix + '_key_macro');
    const textInput = document.getElementById(prefix + '_text_value');
    const textFontInput = document.getElementById(prefix + '_text_value_font');
    const navigateSelect = document.getElementById(prefix + '_navigate_target');
    const switchSelect = document.getElementById(prefix + '_switch_entity');
    const switchStyleSelect = document.getElementById(prefix + '_switch_style');
    const imageSelect = document.getElementById(prefix + '_image_select');
    const imageUrlInput = document.getElementById(prefix + '_image_url');
    const imageIntervalInput = document.getElementById(prefix + '_image_slideshow_sec');
    const imagePreviewCheck = document.getElementById(prefix + '_image_preview');
    const clockTimeCheck = document.getElementById(prefix + '_clock_show_time');
    const clockDateCheck = document.getElementById(prefix + '_clock_show_date');
    const counterInput = document.getElementById(prefix + '_counter_value');

    if (titleInput) titleInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (iconInput) iconInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (colorInput) colorInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (colInput) colInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (rowInput) rowInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (spanWInput) spanWInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (spanHInput) spanHInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (typeSelect) typeSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (entitySelect) entitySelect.addEventListener('change', () => { maybeFillTitleFromSensor(tab); updateTilePreview(tab); updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (weatherSelect) weatherSelect.addEventListener('change', () => { maybeFillTitleFromWeather(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (unitInput) unitInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (decimalsInput) decimalsInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (valueFontSelect) valueFontSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (displayModeSelect) displayModeSelect.addEventListener('change', () => { syncGaugeUi(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeMinInput) gaugeMinInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeMaxInput) gaugeMaxInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeArcInput) gaugeArcInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeSizeInput) gaugeSizeInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
      if (gaugeYOffsetInput) gaugeYOffsetInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
      if (valueYOffsetInput) valueYOffsetInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
      if (graphHeightInput) graphHeightInput.addEventListener('input', () => { updateDraft(tab); scheduleAutoSave(tab); });
    if (sceneInput) sceneInput.addEventListener('input', () => { maybeFillTitleFromScene(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (keyInput) keyInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (textInput) textInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (textFontInput) textFontInput.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (counterInput) counterInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
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
    if (imagePreviewCheck) imagePreviewCheck.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
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
    const typeWas = tileElem.dataset.type || '0';
    const title = document.getElementById(prefix + '_tile_title').value;
    const color = document.getElementById(prefix + '_tile_color').value;
    const type = document.getElementById(prefix + '_tile_type').value;
    const meta = getTileTypeMeta(type);
    const iconInput = document.getElementById(prefix + '_tile_icon');
    const switchStyle = document.getElementById(prefix + '_switch_style')?.value || '0';
    const sensorValueFont = document.getElementById(prefix + '_sensor_value_font')?.value || '0';
    const sensorValueClass = getSensorValueFontClass(sensorValueFont);
    const previewKind = meta.preview || 'none';
    const sensorEntity = document.getElementById(prefix + '_sensor_entity')?.value || '';
    const weatherEntity = document.getElementById(prefix + '_weather_entity')?.value || '';
    const switchEntity = document.getElementById(prefix + '_switch_entity')?.value || '';
    const iconEntity = (previewKind === 'sensor')
      ? sensorEntity
      : (previewKind === 'switch'
        ? switchEntity
        : (previewKind === 'weather' ? weatherEntity : ''));
    const iconName = resolveIconName(iconInput ? iconInput.value : '', iconEntity, sensorMetaCache.icons);

    tileElem.className = 'tile';
    if (meta.css) tileElem.classList.add(meta.css);
    if (type === '5' && switchStyle === '1') tileElem.classList.add('switch-toggle');
    tileElem.style.background = '';
    tileElem.dataset.type = type;

    if (type === '0') {
      tileElem.classList.add('empty');
      tileElem.style.background = 'transparent';
      tileElem.innerHTML = '';
      if (wasActive) tileElem.classList.add('active');
      updateLayoutFromInputs(tab);
      return;
    }

    const defaultBg = meta.defaultBg || '#353535';
    const tileBg = color || defaultBg;
    tileElem.style.background = tileBg;
    tileElem.style.removeProperty('--switch-knob-color');
    tileElem.style.removeProperty('--switch-on-color');
    if (type === '5' && switchStyle === '1') {
      tileElem.style.setProperty('--switch-knob-color', tileBg);
      tileElem.style.setProperty('--switch-on-color', '#3B82F6');
    }

    let html = '';

    if (iconName) {
      html += '<i class="mdi mdi-' + iconName + ' tile-icon"></i>';
    }

    if (title) {
      html += '<div class="tile-title" id="' + tileId + '-title">' + title + '</div>';
    }

    if (previewKind === 'sensor') {
      const entitySelect = document.getElementById(prefix + '_sensor_entity');
      const unitInput = document.getElementById(prefix + '_sensor_unit');
      const entity = entitySelect ? entitySelect.value : '';
      const unit = resolveUnitValue(unitInput ? unitInput.value : '', entity, sensorMetaCache.units);
      html += '<div class="tile-value ' + sensorValueClass + '" id="' + tileId + '-value">--';
      if (unit) html += '<span class="tile-unit">' + unit + '</span>';
      html += '</div>';
      if (entity) {
        tileElem.innerHTML = html;
        if (wasActive) tileElem.classList.add('active');
        updateSensorValuePreview(tab);
      }
    }

    if (previewKind === 'clock') {
      const flags = getClockFlagsFromInputs(prefix);
      if (flags & 1) html += '<div class="tile-clock-time">' + getClockPreviewTime() + '</div>';
      if (flags & 2) html += '<div class="tile-clock-date">' + getClockPreviewDate() + '</div>';
    }

    if (previewKind === 'text') {
      const textValue = document.getElementById(prefix + '_text_value')?.value || '';
      if (textValue) {
        const textFont = document.getElementById(prefix + '_text_value_font')?.value || '0';
        const textClass = getSensorValueFontClass(textFont);
        html += '<div class="tile-text ' + textClass + '">' + textValue + '</div>';
      }
    }

    if (previewKind === 'counter') {
      const counterVal = document.getElementById(prefix + '_counter_value')?.value || '0';
      html += '<div class="tile-counter-value">' + counterVal + '</div>';
    }

    if (previewKind === 'switch' && switchStyle === '1') {
      html += '<div class="tile-switch" id="' + tileId + '-switch"><div class="tile-switch-knob"></div></div>';
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
        resetAllTypeFields(tab);
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
        const meta = getTileTypeMeta(data.type || 0);
        callTypeHandler(meta, 'load', prefix, data);
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
    const meta = getTileTypeMeta(typeValue);
    return !!meta.locked;
  }

  function applySpecialTileUiState(tab) {
    const prefix = tab;
    const typeEl = document.getElementById(prefix + '_tile_type');
    const navSelect = document.getElementById(prefix + '_navigate_target');
    const noteEl = document.getElementById(prefix + '_navigate_note');
    const typeValue = typeEl ? String(typeEl.value) : '0';
    const meta = getTileTypeMeta(typeValue);
    const locked = !!meta.locked;
    if (typeEl) typeEl.disabled = locked;
    if (navSelect) navSelect.disabled = (!meta.fields || meta.fields !== 'navigate' || locked);
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
    const meta = getTileTypeMeta(typeValue);
    if (meta.fields) {
      const fieldsEl = document.getElementById(prefix + '_' + meta.fields + '_fields');
      if (fieldsEl) fieldsEl.classList.add('show');
    }
    if (meta.onSelect) {
      callTypeHandler(meta, 'onSelect', tab);
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

  function resetAllTypeFields(tab) {
    const metas = Object.values(TILE_TYPE_REGISTRY || {});
    metas.forEach(meta => callTypeHandler(meta, 'reset', tab));
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
    resetAllTypeFields(tab);
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
    const meta = getTileTypeMeta(typeValue);
    callTypeHandler(meta, 'save', prefix, formData);
    fetch('/api/tiles', { method:'POST', body:formData })
      .then(res => res.json())
      .then(data => {
        if (data.success) {
          if (!silent) showNotification('Kachel gespeichert & Display aktualisiert!');
          clearDraft(tab, currentTileIndex);
          if (!silent) loadSensorValues(true);
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
      const displayMode = (tile.sensor_display_mode !== undefined) ? tile.sensor_display_mode : (tile.sensor_gauge ? 1 : 0);
      fd.append('sensor_display_mode', String(displayMode));
      if (tile.sensor_gauge_min !== undefined && tile.sensor_gauge_min !== null && String(tile.sensor_gauge_min).length > 0) {
        fd.append('sensor_gauge_min', tile.sensor_gauge_min);
      }
      if (tile.sensor_gauge_max !== undefined && tile.sensor_gauge_max !== null && String(tile.sensor_gauge_max).length > 0) {
        fd.append('sensor_gauge_max', tile.sensor_gauge_max);
      }
      if (tile.sensor_gauge_arc !== undefined && tile.sensor_gauge_arc !== null && String(tile.sensor_gauge_arc).length > 0) {
        fd.append('sensor_gauge_arc', tile.sensor_gauge_arc);
      }
      if (tile.sensor_gauge_size !== undefined && tile.sensor_gauge_size !== null && String(tile.sensor_gauge_size).length > 0) {
        fd.append('sensor_gauge_size', tile.sensor_gauge_size);
      }
      if (tile.sensor_gauge_y_offset !== undefined && tile.sensor_gauge_y_offset !== null && String(tile.sensor_gauge_y_offset).length > 0) {
        fd.append('sensor_gauge_y_offset', tile.sensor_gauge_y_offset);
      }
      if (tile.sensor_value_y_offset !== undefined && tile.sensor_value_y_offset !== null && String(tile.sensor_value_y_offset).length > 0) {
        fd.append('sensor_value_y_offset', tile.sensor_value_y_offset);
      }
      if (tile.sensor_graph_height !== undefined && tile.sensor_graph_height !== null && String(tile.sensor_graph_height).length > 0) {
        fd.append('sensor_graph_height', tile.sensor_graph_height);
      }
    } else if (safeType === 2) {
      fd.append('scene_alias', tile.scene_alias || '');
      fd.append('image_path', tile.image_path || '');
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
      const preview = (tile.image_preview !== undefined && tile.image_preview !== null)
        ? tile.image_preview
        : (tile.sensor_display_mode !== undefined && tile.sensor_display_mode !== null ? tile.sensor_display_mode : 0);
      fd.append('image_preview', preview ? '1' : '0');
    } else if (safeType === 10) {
      fd.append('text_value', tile.text_value || tile.scene_alias || tile.key_macro || '');
      fd.append('text_value_font', tile.text_value_font || tile.sensor_value_font || '0');
    } else if (safeType === 11) {
      fd.append('counter_value', tile.counter_value || tile.scene_alias || '0');
    } else if (safeType === 12) {
      fd.append('weather_entity', tile.sensor_entity || tile.weather_entity || '');
    }

    const res = await fetch('/api/tiles', { method: 'POST', body: fd });
    const data = await res.json();
    if (!data.success) {
      throw new Error('Tile speichern fehlgeschlagen');
    }
  }

  function rgbToHex(rgb) { return '#' + ('000000' + rgb.toString(16)).slice(-6); }
  function hexToRgb(hex) { return parseInt(hex.replace('#', ''), 16); }

  function renderTileFromData(tab, index, tile, sensorMeta) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return;
    const metaValues = sensorMeta?.values || {};
    const metaUnits = sensorMeta?.units || {};
    const metaIcons = sensorMeta?.icons || {};
    el.dataset.index = index.toString();
    const typeValue = String(tile?.type ?? '0');
    const meta = getTileTypeMeta(typeValue);
    let cls = ['tile'];
    if (meta.css) cls.push(meta.css);
    if (typeValue === '5' && tile.switch_style === 1) cls.push('switch-toggle');
    if (typeValue === '0' && (!meta.css || meta.css !== 'empty')) cls.push('empty');
    el.className = cls.join(' ');
    el.dataset.type = typeValue;
    if (typeValue === '0') el.style.background = 'transparent';
    else {
      const bg = tile.bg_color ? rgbToHex(tile.bg_color) : (meta.defaultBg || '#353535');
      el.style.background = bg;
      el.style.removeProperty('--switch-knob-color');
      el.style.removeProperty('--switch-on-color');
      if (typeValue === '5' && tile.switch_style === 1) {
        el.style.setProperty('--switch-knob-color', bg);
        el.style.setProperty('--switch-on-color', '#3B82F6');
      }
    }
    const sensorValueClass = getSensorValueFontClass(tile.sensor_value_font);
    if (typeValue === '0') { el.innerHTML = ''; }
    else {
      const previewKind = meta.preview || 'none';
      const iconEntity = (previewKind === 'sensor' || previewKind === 'switch' || previewKind === 'weather')
        ? (tile.sensor_entity || '')
        : '';
      const iconName = resolveIconName(tile.icon_name || '', iconEntity, metaIcons);

      let html = '';

      if (iconName) {
        html += '<i class="mdi mdi-' + iconName + ' tile-icon"></i>';
      }

      if (tile.title && tile.title.length) {
        html += '<div class="tile-title" id="' + tab + '-tile-' + index + '-title">' + tile.title + '</div>';
      }

      if (previewKind === 'sensor') {
        let value = '--';
        if (tile.sensor_entity) value = formatSensorValue(metaValues[tile.sensor_entity] ?? '--', tile.sensor_decimals);
        const unit = resolveUnitValue(tile.sensor_unit || '', tile.sensor_entity || '', metaUnits);
        html += '<div class="tile-value ' + sensorValueClass + '" id="' + tab + '-tile-' + index + '-value">' + value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '') + '</div>';
      }
      if (previewKind === 'clock') {
        const flags = normalizeClockFlags(tile.sensor_decimals);
        if (flags & 1) html += '<div class="tile-clock-time">' + getClockPreviewTime() + '</div>';
        if (flags & 2) html += '<div class="tile-clock-date">' + getClockPreviewDate() + '</div>';
      }
      if (previewKind === 'text') {
        const textValue = tile.text_value || tile.scene_alias || tile.key_macro || '';
        if (textValue) {
          const textClass = getSensorValueFontClass(tile.sensor_value_font);
          html += '<div class="tile-text ' + textClass + '">' + textValue + '</div>';
        }
      }
      if (previewKind === 'counter') {
        const counterVal = tile.counter_value || tile.scene_alias || '0';
        html += '<div class="tile-counter-value">' + counterVal + '</div>';
      }
      if (previewKind === 'switch' && tile.switch_style === 1) {
        html += '<div class="tile-switch" id="' + tab + '-tile-' + index + '-switch"><div class="tile-switch-knob"></div></div>';
      }
      el.innerHTML = html;
    }
    if (currentTileTab === tab && currentTileIndex === index) el.classList.add('active');
    if (typeValue === '5' && tile.sensor_entity) {
      const state = parseSwitchPayload(metaValues[tile.sensor_entity] ?? '');
      applySwitchPreviewState(el, state);
    }
  }

  function loadSensorValues(refreshTiles = false) {
    const tabs = tileTabs.slice();
    const tileRequests = refreshTiles
      ? tabs.map(tab => {
          const folderId = getFolderIdForTab(tab);
          if (folderId === undefined) return Promise.resolve([]);
          return fetch('/api/tiles?folder=' + encodeURIComponent(folderId))
            .then(res => res.json())
            .catch(() => []);
        })
      : tabs.map(tab => Promise.resolve(getTilesData(tab)));

    Promise.all([fetchSensorMetaCache(), ...tileRequests])
    .then(results => {
      const sensorMeta = normalizeSensorMetaPayload(results[0] || {});
      sensorMetaCache = sensorMeta;
      tabs.forEach((tab, idx) => {
        const tiles = Array.isArray(results[idx + 1]) ? results[idx + 1] : [];
        if (refreshTiles) {
          tilesData[tab] = tiles;
        }
        const tilesForRender = refreshTiles ? tiles : getTilesData(tab);
        if (!Array.isArray(tilesForRender)) return;
        tilesForRender.forEach((tile, i) => renderTileFromData(tab, i, tile, sensorMeta));
        layoutTiles(tab, tilesForRender);
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
          loadSensorValues(false);
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
    loadSensorValues(true);
    let savedTab = null;
    try { savedTab = localStorage.getItem('activeAdminTab'); } catch (e) {}
    const defaultTab = tileTabs.length ? ('tab-tiles-' + tileTabs[0]) : 'tab-network';
    const targetTab = savedTab && document.getElementById(savedTab) ? savedTab : defaultTab;
    const targetBtn = Array.from(document.querySelectorAll('.tab-btn')).find(btn => btn.getAttribute('onclick')?.includes(targetTab)) || document.querySelector('.tab-btn');
    if (targetBtn) targetBtn.click();
    setInterval(() => loadSensorValues(false), 5000);
    tileTabs.forEach(tab => enableTileDrag(tab));
  });
  </script>
)html";

  append_tile_type_scripts(html);
}



