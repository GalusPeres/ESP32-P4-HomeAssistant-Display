#include "src/web/web_admin_scripts.h"

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

  // Gespeicherte Tab-Daten (verhindert Überschreiben bei schnellem Tippen)
  const savedTabData = {
    0: {name: '', icon: ''},
    1: {name: '', icon: ''},
    2: {name: '', icon: ''},
    3: {name: '', icon: ''}
  };

  // Initialisierung beim Laden der Seite
  function initSavedTabData() {
    console.log('[DEBUG] initSavedTabData() called');
    for (let i = 0; i < 4; i++) {
      const nameInput = document.getElementById('tab' + i + '_tab_name');
      const iconInput = document.getElementById('tab' + i + '_tab_icon');
      if (nameInput) savedTabData[i].name = nameInput.value.trim();
      if (iconInput) savedTabData[i].icon = iconInput.value.trim();
      console.log('[DEBUG] Tab ' + i + ' initialized: name="' + savedTabData[i].name + '", icon="' + savedTabData[i].icon + '"');
    }
  }

  // Debounce Helper (500ms Verzögerung) - NUR EINER für beide Felder!
  const tabSaveTimers = {};

  function debouncedSaveTab(tabIndex) {
    // Cache SOFORT aktualisieren aus Input-Feldern
    const nameInput = document.getElementById('tab' + tabIndex + '_tab_name');
    const iconInput = document.getElementById('tab' + tabIndex + '_tab_icon');

    console.log('[DEBUG] debouncedSaveTab(' + tabIndex + '): nameInput found=' + (nameInput !== null) + ', value="' + (nameInput ? nameInput.value : 'NULL') + '"');
    console.log('[DEBUG] debouncedSaveTab(' + tabIndex + '): iconInput found=' + (iconInput !== null) + ', value="' + (iconInput ? iconInput.value : 'NULL') + '"');

    savedTabData[tabIndex].name = nameInput ? nameInput.value.trim() : '';
    savedTabData[tabIndex].icon = iconInput ? iconInput.value.trim() : '';
    console.log('[DEBUG] debouncedSaveTab(' + tabIndex + '): Cache updated to name="' + savedTabData[tabIndex].name + '", icon="' + savedTabData[tabIndex].icon + '"');

    // Clear existing timer for this tab
    if (tabSaveTimers[tabIndex]) {
      clearTimeout(tabSaveTimers[tabIndex]);
    }
    // Set new timer - EINE Funktion für beide Felder!
    tabSaveTimers[tabIndex] = setTimeout(() => {
      saveTab(tabIndex);
    }, 500);
  }

  // Diese Funktionen rufen die gemeinsame debounced Funktion auf
  function debouncedSaveTabName(tabIndex, newName) {
    debouncedSaveTab(tabIndex);
  }

  function debouncedSaveTabIcon(tabIndex, newIcon) {
    debouncedSaveTab(tabIndex);
  }

  // EINE gemeinsame Save-Funktion für beide Felder (wie bei Tiles!)
  function saveTab(tabIndex) {
    const tabName = savedTabData[tabIndex].name;
    const iconName = savedTabData[tabIndex].icon;
    console.log('[DEBUG] saveTab(' + tabIndex + '): name="' + tabName + '", icon="' + iconName + '"');

    fetch('/api/tabs/rename', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'tab=' + tabIndex + '&name=' + encodeURIComponent(tabName) + '&icon_name=' + encodeURIComponent(iconName)
    })
    .then(r => r.json())
    .then(data => {
      if (data.success) {
        updateTabButton(tabIndex, tabName, iconName);
        console.log('Tab ' + tabIndex + ' saved: name="' + tabName + '", icon="' + iconName + '"');
      } else {
        console.error('Error updating tab:', data.error);
        alert('Fehler beim Aktualisieren: ' + (data.error || 'Unbekannt'));
      }
    })
    .catch(e => {
      console.error('Error updating tab:', e);
      alert('Fehler beim Aktualisieren: ' + e);
    });
  }

  function updateTabButton(tabIndex, tabName, iconName) {
    console.log('[DEBUG] updateTabButton(' + tabIndex + '): tabName="' + tabName + '", iconName="' + iconName + '"');

    // Update tab button to show icon and name (beide optional)
    const tabNameSpan = document.getElementById('tab-name-' + tabIndex);
    let tabBtn;

    if (tabNameSpan) {
      tabBtn = tabNameSpan.parentElement;
    } else {
      // Fallback: Button direkt finden über Attribut-Selektor
      const allBtns = document.querySelectorAll('.tab-btn');
      if (allBtns[tabIndex]) {
        tabBtn = allBtns[tabIndex];
      }
    }

    if (!tabBtn) return;

    // Normalize icon name
    let normalizedIcon = (iconName || '').trim().toLowerCase();
    if (normalizedIcon.startsWith('mdi:')) normalizedIcon = normalizedIcon.substring(4);
    else if (normalizedIcon.startsWith('mdi-')) normalizedIcon = normalizedIcon.substring(4);

    // Clear existing content
    tabBtn.innerHTML = '';

    const hasIcon = normalizedIcon && normalizedIcon.length > 0;
    const hasName = tabName && tabName.trim().length > 0;
    console.log('[DEBUG] updateTabButton(' + tabIndex + '): hasIcon=' + hasIcon + ', hasName=' + hasName + ', normalizedIcon="' + normalizedIcon + '"');

    // Add icon if present
    if (hasIcon) {
      const iconEl = document.createElement('i');
      iconEl.className = 'mdi mdi-' + normalizedIcon;
      iconEl.style.cssText = 'font-size:24px;';
      tabBtn.appendChild(iconEl);
    }

    // Add text, or fallback number if both empty
    if (hasName) {
      const textSpan = document.createElement('span');
      textSpan.id = 'tab-name-' + tabIndex;
      textSpan.textContent = tabName;
      textSpan.style.cssText = 'font-size:14px;font-weight:600;';
      tabBtn.appendChild(textSpan);
    } else if (!hasIcon && tabIndex < 3) {
      // Fallback: Wenn beides leer, zeige Nummer "1", "2", "3"
      const textSpan = document.createElement('span');
      textSpan.id = 'tab-name-' + tabIndex;
      textSpan.textContent = String(tabIndex + 1);
      textSpan.style.cssText = 'font-size:14px;font-weight:600;';
      tabBtn.appendChild(textSpan);
    } else {
      // Leerer Span für ID (damit Button später gefunden werden kann)
      const textSpan = document.createElement('span');
      textSpan.id = 'tab-name-' + tabIndex;
      textSpan.textContent = '';
      textSpan.style.cssText = 'display:none;';
      tabBtn.appendChild(textSpan);
    }
  }

  // Tile Editor State
  const tileTabs = ['tab0', 'tab1', 'tab2'];
  let currentTileTab = 'tab0';
  let currentTileIndex = -1;
  let drafts = { tab0: {}, tab1: {}, tab2: {} };
  let tab0TilesData = [];
  let tab1TilesData = [];
  let tab2TilesData = [];

  function persistDrafts() { try { localStorage.setItem('tileDrafts', JSON.stringify(drafts)); } catch (e) {} }
  function loadDraftsFromStorage() {
    try {
      const raw = localStorage.getItem('tileDrafts');
      if (raw) drafts = JSON.parse(raw);
    } catch (e) {
      drafts = { tab0: {}, tab1: {}, tab2: {} };
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
      sensor_entity: document.getElementById(prefix + '_sensor_entity')?.value || '',
      sensor_unit: document.getElementById(prefix + '_sensor_unit')?.value || '',
      sensor_decimals: document.getElementById(prefix + '_sensor_decimals')?.value || '',
      scene_alias: document.getElementById(prefix + '_scene_alias')?.value || '',
      key_macro: document.getElementById(prefix + '_key_macro')?.value || '',
      navigate_target: document.getElementById(prefix + '_navigate_target')?.value || '0',
      switch_entity: document.getElementById(prefix + '_switch_entity')?.value || '',
      switch_style: document.getElementById(prefix + '_switch_style')?.value || '0'
    };
    drafts[tab][currentTileIndex] = d;
    persistDrafts();
  }

  function applyDraft(tab, index) {
    const d = drafts[tab] && drafts[tab][index];
    if (!d) return false;
    const prefix = tab;
    document.getElementById(prefix + '_tile_type').value = d.type || '0';
    updateTileType(tab);
    document.getElementById(prefix + '_tile_title').value = d.title || '';
    document.getElementById(prefix + '_tile_icon').value = d.icon || '';
    document.getElementById(prefix + '_tile_color').value = d.color || '#2A2A2A';
    if (d.type === '1') {
      document.getElementById(prefix + '_sensor_entity').value = d.sensor_entity || '';
      document.getElementById(prefix + '_sensor_unit').value = d.sensor_unit || '';
      const decEl = document.getElementById(prefix + '_sensor_decimals');
      if (decEl) decEl.value = d.sensor_decimals || '';
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
      sensor_entity: document.getElementById(prefix + '_sensor_entity')?.value || '',
      sensor_unit: document.getElementById(prefix + '_sensor_unit')?.value || '',
      sensor_decimals: document.getElementById(prefix + '_sensor_decimals')?.value || '',
      scene_alias: document.getElementById(prefix + '_scene_alias')?.value || '',
      key_macro: document.getElementById(prefix + '_key_macro')?.value || '',
      navigate_target: document.getElementById(prefix + '_navigate_target')?.value || '0',
      switch_entity: document.getElementById(prefix + '_switch_entity')?.value || '',
      switch_style: document.getElementById(prefix + '_switch_style')?.value || '0'
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

    const sensorEntityEl = document.getElementById(prefix + '_sensor_entity');
    if (sensorEntityEl) sensorEntityEl.value = data.sensor_entity || '';
    const sensorUnitEl = document.getElementById(prefix + '_sensor_unit');
    if (sensorUnitEl) sensorUnitEl.value = data.sensor_unit || '';
    const sensorDecEl = document.getElementById(prefix + '_sensor_decimals');
    if (sensorDecEl) sensorDecEl.value = data.sensor_decimals || '';

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

  function maybeFillTitleFromScene(tab) {
    const prefix = tab;
    const typeSel = document.getElementById(prefix + '_tile_type');
    const titleInput = document.getElementById(prefix + '_tile_title');
    const sceneSel = document.getElementById(prefix + '_scene_alias');
    if (!typeSel || !titleInput || !sceneSel) return;
    if (typeSel.value !== '2') return;
    if (titleInput.value && titleInput.value.trim().length) return;
    const opt = sceneSel.selectedOptions && sceneSel.selectedOptions[0];
    if (!opt) return;
    const label = opt.textContent || opt.innerText || '';
    const title = label.split(' - ')[0] || opt.value || '';
    if (title.trim().length) titleInput.value = title.trim();
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

  function maybeFillTitleFromSensor(tab) {
    maybeFillTitleFromEntity(tab, '_sensor_entity');
  }

  function maybeFillTitleFromSwitch(tab) {
    maybeFillTitleFromEntity(tab, '_switch_entity');
  }

  function setupLivePreview(tab) {
    const prefix = tab;
    const fields = [
      '_tile_title','_tile_color','_tile_type','_sensor_entity','_sensor_unit',
      '_sensor_decimals','_scene_alias','_key_macro','_navigate_target','_switch_entity','_switch_style'
    ];
    fields.forEach(id => {
      const el = document.getElementById(prefix + id);
      if (el) el.replaceWith(el.cloneNode(true));
    });

    const titleInput = document.getElementById(prefix + '_tile_title');
    const iconInput = document.getElementById(prefix + '_tile_icon');
    const colorInput = document.getElementById(prefix + '_tile_color');
    const typeSelect = document.getElementById(prefix + '_tile_type');
    const entitySelect = document.getElementById(prefix + '_sensor_entity');
    const unitInput = document.getElementById(prefix + '_sensor_unit');
    const decimalsInput = document.getElementById(prefix + '_sensor_decimals');
    const sceneInput = document.getElementById(prefix + '_scene_alias');
    const keyInput = document.getElementById(prefix + '_key_macro');
    const navigateSelect = document.getElementById(prefix + '_navigate_target');
    const switchSelect = document.getElementById(prefix + '_switch_entity');
    const switchStyleSelect = document.getElementById(prefix + '_switch_style');

    if (titleInput) titleInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (iconInput) iconInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (colorInput) colorInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (typeSelect) typeSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (entitySelect) entitySelect.addEventListener('change', () => { maybeFillTitleFromSensor(tab); updateTilePreview(tab); updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (unitInput) unitInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (decimalsInput) decimalsInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (sceneInput) sceneInput.addEventListener('input', () => { maybeFillTitleFromScene(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (keyInput) keyInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (navigateSelect) navigateSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (switchSelect) switchSelect.addEventListener('change', () => { maybeFillTitleFromSwitch(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (switchStyleSelect) switchStyleSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
  }

  function formatSensorValue(value, decimals) {
    if (value === undefined || value === null) return '--';
    let text = String(value).trim();
    if (!text.length) return '--';
    const lower = text.toLowerCase();
    if (lower === 'unavailable' || lower === 'unknown' || lower === 'none') return '--';
    if (decimals === undefined || decimals === null || decimals === '' || Number(decimals) === -1) return text;
    const num = parseFloat(text.replace(',', '.'));
    if (isNaN(num) || !isFinite(num)) return text;
    const d = Math.max(0, Math.min(6, parseInt(decimals, 10) || 0));
    return num.toFixed(d);
  }

  const SWITCH_ICON_ON = '#FFD54F';
  const SWITCH_ICON_OFF = '#B0B0B0';

  function parseOnOff(text) {
    const lower = String(text || '').trim().toLowerCase();
    if (['on', 'true', '1', 'yes'].includes(lower)) return true;
    if (['off', 'false', '0', 'no'].includes(lower)) return false;
    return null;
  }

  function parseHexColor(text) {
    let t = String(text || '').trim();
    if (t.startsWith('#')) t = t.substring(1);
    if (t.startsWith('0x') || t.startsWith('0X')) t = t.substring(2);
    if (t.length !== 6) return null;
    if (!/^[0-9a-fA-F]{6}$/.test(t)) return null;
    return '#' + t.toLowerCase();
  }

  function rgbToHexColor(r, g, b) {
    const clamp = (v) => Math.max(0, Math.min(255, v));
    const rr = clamp(r).toString(16).padStart(2, '0');
    const gg = clamp(g).toString(16).padStart(2, '0');
    const bb = clamp(b).toString(16).padStart(2, '0');
    return '#' + rr + gg + bb;
  }

  function parseRgbList(list) {
    if (Array.isArray(list) && list.length >= 3) {
      return rgbToHexColor(Number(list[0]), Number(list[1]), Number(list[2]));
    }
    const parts = String(list || '').split(',');
    if (parts.length < 3) return null;
    return rgbToHexColor(parseInt(parts[0], 10), parseInt(parts[1], 10), parseInt(parts[2], 10));
  }

  function hsToRgb(h, s) {
    const hh = ((h % 360) + 360) % 360;
    const sat = Math.max(0, Math.min(100, s)) / 100;
    const c = sat;
    const x = c * (1 - Math.abs((hh / 60) % 2 - 1));
    const m = 1 - c;
    let r1 = 0, g1 = 0, b1 = 0;
    if (hh < 60) { r1 = c; g1 = x; b1 = 0; }
    else if (hh < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (hh < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (hh < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (hh < 300) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }
    return rgbToHexColor(Math.round((r1 + m) * 255), Math.round((g1 + m) * 255), Math.round((b1 + m) * 255));
  }

  function parseSwitchPayload(value) {
    const out = { hasState: false, isOn: false, hasColor: false, color: null };
    if (value === undefined || value === null) return out;
    const text = String(value).trim();
    if (!text.length) return out;

    if (text.startsWith('{')) {
      try {
        const obj = JSON.parse(text);
        if (obj && typeof obj === 'object') {
          if (obj.state !== undefined) {
            const on = parseOnOff(obj.state);
            if (on !== null) {
              out.hasState = true;
              out.isOn = on;
            }
          }
          if (obj.color) {
            const hex = parseHexColor(obj.color);
            if (hex) {
              out.hasColor = true;
              out.color = hex;
            }
          }
          if (!out.hasColor && obj.rgb_color) {
            const hex = parseRgbList(obj.rgb_color);
            if (hex) {
              out.hasColor = true;
              out.color = hex;
            }
          }
          if (!out.hasColor && obj.hs_color && Array.isArray(obj.hs_color) && obj.hs_color.length >= 2) {
            out.hasColor = true;
            out.color = hsToRgb(Number(obj.hs_color[0]), Number(obj.hs_color[1]));
          }
        }
      } catch (e) {}
    }

    if (!out.hasState) {
      const on = parseOnOff(text);
      if (on !== null) {
        out.hasState = true;
        out.isOn = on;
      }
    }

    if (!out.hasColor) {
      const hex = parseHexColor(text);
      if (hex) {
        out.hasColor = true;
        out.color = hex;
      } else if (text.startsWith('rgb(') && text.endsWith(')')) {
        const hexRgb = parseRgbList(text.substring(4, text.length - 1));
        if (hexRgb) {
          out.hasColor = true;
          out.color = hexRgb;
        }
      }
    }

    if (!out.hasState && out.hasColor) {
      out.hasState = true;
      out.isOn = true;
    }
    return out;
  }

  function applySwitchPreviewState(tileElem, state) {
    if (!tileElem) return;
    if (!state.hasState && !state.hasColor) return;
    const iconEl = tileElem.querySelector('.tile-icon');
    const switchEl = tileElem.querySelector('.tile-switch');
    let isOn = state.hasState ? state.isOn : state.hasColor;
    let color = SWITCH_ICON_OFF;
    if (isOn) color = state.hasColor ? state.color : SWITCH_ICON_ON;
    if (iconEl) iconEl.style.color = color;
    if (switchEl) {
      if (isOn) switchEl.classList.add('is-on');
      else switchEl.classList.remove('is-on');
      if (isOn && state.hasColor) switchEl.style.setProperty('--switch-on-color', state.color);
      else switchEl.style.removeProperty('--switch-on-color');
    }
  }

  function updateSensorValuePreview(tab) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const entitySelect = document.getElementById(prefix + '_sensor_entity');
    const unitInput = document.getElementById(prefix + '_sensor_unit');
    const decimalsInput = document.getElementById(prefix + '_sensor_decimals');
    if (!entitySelect) return;
    const entity = entitySelect.value;
    if (!entity) {
      const valueElem = document.getElementById(tab + '-tile-' + currentTileIndex + '-value');
      if (valueElem) {
        const unit = unitInput ? unitInput.value : '';
        valueElem.innerHTML = '--' + (unit ? '<span class="tile-unit">' + unit + '</span>' : '');
      }
      return;
    }
    fetch('/api/sensor_values')
      .then(res => res.json())
      .then(values => {
        const valueElem = document.getElementById(tab + '-tile-' + currentTileIndex + '-value');
        if (valueElem) {
          const decimals = decimalsInput ? decimalsInput.value : '';
          let value = formatSensorValue(values[entity] ?? '--', decimals);
          const unit = unitInput ? unitInput.value : '';
          valueElem.innerHTML = value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '');
        }
      })
      .catch(err => console.error('Fehler beim Laden des Sensorwerts:', err));
  }

  function updateSwitchValuePreview(tab) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const entitySelect = document.getElementById(prefix + '_switch_entity');
    if (!entitySelect) return;
    const entity = entitySelect.value;
    const tileElem = document.getElementById(tab + '-tile-' + currentTileIndex);
    if (!entity || !tileElem) return;
    fetch('/api/sensor_values')
      .then(res => res.json())
      .then(values => {
        const state = parseSwitchPayload(values[entity] ?? '');
        applySwitchPreviewState(tileElem, state);
      })
      .catch(err => console.error('Fehler beim Laden des Switch-Status:', err));
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
                    tileElem.classList.contains('switch')   ? '5' : '0';
    const title = document.getElementById(prefix + '_tile_title').value;
    const color = document.getElementById(prefix + '_tile_color').value;
    const type = document.getElementById(prefix + '_tile_type').value;
    const iconInput = document.getElementById(prefix + '_tile_icon');
    const switchStyle = document.getElementById(prefix + '_switch_style')?.value || '0';
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
      return;
    } else if (type === '1') {
      tileElem.classList.add('sensor');
      tileElem.style.background = color || '#2A2A2A';
    } else if (type === '2') {
      tileElem.classList.add('scene');
      tileElem.style.background = color || '#353535';
    } else if (type === '3') {
      tileElem.classList.add('key');
      tileElem.style.background = color || '#353535';
    } else if (type === '4') {
      tileElem.classList.add('navigate');
      tileElem.style.background = color || '#353535';
    } else if (type === '5') {
      tileElem.classList.add('switch');
      if (switchStyle === '1') tileElem.classList.add('switch-toggle');
      tileElem.style.background = color || '#353535';
    }

    let html = '';

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
      html += '<div class="tile-value" id="' + tileId + '-value">--';
      if (unit) html += '<span class="tile-unit">' + unit + '</span>';
      html += '</div>';
      if (entity) {
        tileElem.innerHTML = html;
        if (wasActive) tileElem.classList.add('active');
        updateSensorValuePreview(tab);
        return;
      }
    }

    if (type === '5' && switchStyle === '1') {
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
}

  function loadTileData(index, tab) {
    fetch('/api/tiles?tab=' + tab + '&index=' + index)
      .then(res => res.json())
      .then(data => {
        const prefix = tab;
        document.getElementById(prefix + '_tile_type').value = data.type || 0;
        updateTileType(tab);
        document.getElementById(prefix + '_tile_title').value = data.title || '';
        document.getElementById(prefix + '_tile_icon').value = data.icon_name || '';
        document.getElementById(prefix + '_tile_color').value = rgbToHex(data.bg_color || 0x2A2A2A);
        if (data.type === 1) {
          document.getElementById(prefix + '_sensor_entity').value = data.sensor_entity || '';
          document.getElementById(prefix + '_sensor_unit').value = data.sensor_unit || '';
          const decEl = document.getElementById(prefix + '_sensor_decimals');
          if (decEl) decEl.value = (data.sensor_decimals !== undefined && data.sensor_decimals >= 0) ? data.sensor_decimals : '';
        } else if (data.type === 2) {
          document.getElementById(prefix + '_scene_alias').value = data.scene_alias || '';
          maybeFillTitleFromScene(tab);
        } else if (data.type === 3) {
          document.getElementById(prefix + '_key_macro').value = data.key_macro || '';
        } else if (data.type === 4) {
          const navEl = document.getElementById(prefix + '_navigate_target');
          if (navEl) navEl.value = (data.sensor_decimals !== undefined && data.sensor_decimals <= 2) ? data.sensor_decimals : '0';
        } else if (data.type === 5) {
          document.getElementById(prefix + '_switch_entity').value = data.sensor_entity || '';
          const styleEl = document.getElementById(prefix + '_switch_style');
          if (styleEl) styleEl.value = (data.switch_style !== undefined) ? String(data.switch_style) : '0';
        }
        const decEl = document.getElementById(prefix + '_sensor_decimals');
        if (data.type !== 1 && decEl) decEl.value = '';
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

  function updateTileType(tab) {
    const prefix = tab;
    const typeValue = document.getElementById(prefix + '_tile_type').value;
    document.querySelectorAll('#' + prefix + 'Settings .type-fields').forEach(f => f.classList.remove('show'));
    if (typeValue === '1') document.getElementById(prefix + '_sensor_fields').classList.add('show');
    else if (typeValue === '2') document.getElementById(prefix + '_scene_fields').classList.add('show');
    else if (typeValue === '3') document.getElementById(prefix + '_key_fields').classList.add('show');
    else if (typeValue === '4') document.getElementById(prefix + '_navigate_fields').classList.add('show');
    else if (typeValue === '5') document.getElementById(prefix + '_switch_fields').classList.add('show');
  }

  function showNotification(message, success = true) {
    const notification = document.getElementById('notification');
    notification.textContent = message;
    notification.style.background = success ? '#10b981' : '#ef4444';
    notification.classList.add('show');
    setTimeout(() => { notification.classList.remove('show'); }, 3000);
  }

  let autoSaveTimers = { tab0: null, tab1: null, tab2: null };
  function scheduleAutoSave(tab) {
    if (autoSaveTimers[tab]) clearTimeout(autoSaveTimers[tab]);
    autoSaveTimers[tab] = setTimeout(() => saveTile(tab, true), 250);
  }

  function resetTile(tab) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    document.getElementById(prefix + '_tile_type').value = '0';
    document.getElementById(prefix + '_tile_title').value = '';
    document.getElementById(prefix + '_tile_icon').value = '';
    document.getElementById(prefix + '_tile_color').value = '#2A2A2A';
    ['_sensor_entity','_sensor_unit','_sensor_decimals','_scene_alias','_key_macro','_switch_entity','_switch_style'].forEach(suf => {
      const el = document.getElementById(prefix + suf);
      if (el) el.value = (suf === '_switch_style') ? '0' : '';
    });
    updateTileType(tab);
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
  }

  function saveTile(tab, silent = false) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const formData = new FormData();
    formData.append('tab', tab);
    formData.append('index', currentTileIndex);
    formData.append('type', document.getElementById(prefix + '_tile_type').value);
    formData.append('title', document.getElementById(prefix + '_tile_title').value);
    formData.append('icon_name', document.getElementById(prefix + '_tile_icon').value);
    formData.append('bg_color', hexToRgb(document.getElementById(prefix + '_tile_color').value));
    const typeValue = document.getElementById(prefix + '_tile_type').value;
    if (typeValue === '1') {
      formData.append('sensor_entity', document.getElementById(prefix + '_sensor_entity').value);
      formData.append('sensor_unit', document.getElementById(prefix + '_sensor_unit').value);
      formData.append('sensor_decimals', document.getElementById(prefix + '_sensor_decimals').value);
    } else if (typeValue === '2') {
      formData.append('scene_alias', document.getElementById(prefix + '_scene_alias').value);
    } else if (typeValue === '3') {
      formData.append('key_macro', document.getElementById(prefix + '_key_macro').value);
    } else if (typeValue === '4') {
      const navTargetElement = document.getElementById(prefix + '_navigate_target');
      const navTargetValue = navTargetElement ? navTargetElement.value : 'ELEMENT_NOT_FOUND';
      console.log('[DEBUG] Navigate Target - Element:', navTargetElement, 'Value:', navTargetValue, 'Prefix:', prefix);
      formData.append('navigate_target', navTargetValue);
    } else if (typeValue === '5') {
      formData.append('switch_entity', document.getElementById(prefix + '_switch_entity').value);
      const styleEl = document.getElementById(prefix + '_switch_style');
      formData.append('switch_style', styleEl ? styleEl.value : '0');
    }
    fetch('/api/tiles', { method:'POST', body:formData })
      .then(res => res.json())
      .then(data => {
        if (data.success) {
          if (!silent) showNotification('Kachel gespeichert & Display aktualisiert!');
          clearDraft(tab, currentTileIndex);
          loadSensorValues();
        } else {
          showNotification('Fehler: ' + (data.error || 'Unbekannt'), false);
        }
      })
      .catch(() => showNotification('Netzwerkfehler beim Speichern', false));
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
    else if (tile.type === 4) cls.push('navigate');
    else if (tile.type === 5) {
      cls.push('switch');
      if (tile.switch_style === 1) cls.push('switch-toggle');
    } else cls.push('empty');
    el.className = cls.join(' ');
    if (tile.type === 0) el.style.background = 'transparent';
    else if (tile.type === 1) el.style.background = tile.bg_color ? ('#' + ('000000' + tile.bg_color.toString(16)).slice(-6)) : '#2A2A2A';
    else el.style.background = tile.bg_color ? ('#' + ('000000' + tile.bg_color.toString(16)).slice(-6)) : '#353535';
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
        html += '<div class="tile-value" id="' + tab + '-tile-' + index + '-value">' + value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '') + '</div>';
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
    Promise.all([
      fetch('/api/sensor_values').then(res => res.json()),
      fetch('/api/tiles?tab=tab0').then(res => res.json()),
      fetch('/api/tiles?tab=tab1').then(res => res.json()),
      fetch('/api/tiles?tab=tab2').then(res => res.json())
    ])
    .then(([sensorValues, tab0Tiles, tab1Tiles, tab2Tiles]) => {
      tab0TilesData = tab0Tiles;
      tab1TilesData = tab1Tiles;
      tab2TilesData = tab2Tiles;
      tab0Tiles.forEach((tile, idx) => renderTileFromData('tab0', idx, tile, sensorValues));
      tab1Tiles.forEach((tile, idx) => renderTileFromData('tab1', idx, tile, sensorValues));
      tab2Tiles.forEach((tile, idx) => renderTileFromData('tab2', idx, tile, sensorValues));
      if (currentTileIndex !== -1) {
        const tab = currentTileTab;
        const settingsId = tab + 'Settings';
        document.getElementById(settingsId)?.classList.remove('hidden');
        const activeTile = document.getElementById(tab + '-tile-' + currentTileIndex);
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
        reorderTiles(dragSource.tab, dragSource.index, targetIndex);
      });
    });
  }

  function reorderTiles(tab, fromIdx, toIdx) {
    fetch('/api/tiles/reorder', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'tab=' + encodeURIComponent(tab) + '&from=' + encodeURIComponent(fromIdx) + '&to=' + encodeURIComponent(toIdx)
    })
    .then(res => res.json())
    .then(data => {
      if (data.success) {
        if (tab === currentTileTab) {
          if (currentTileIndex === fromIdx) currentTileIndex = toIdx;
          else if (currentTileIndex === toIdx) currentTileIndex = fromIdx;
        }
        swapDrafts(tab, fromIdx, toIdx);
        showNotification('Kacheln verschoben & gespeichert!');
        loadSensorValues();
      } else {
        showNotification('Fehler beim Verschieben', false);
      }
    })
    .catch(() => showNotification('Netzwerkfehler beim Verschieben', false));
  }

  function loadTileDataAndSelect(tab, index) { selectTile(index, tab); }

  document.addEventListener('DOMContentLoaded', () => {
    // Verzögerte Initialisierung, damit Input-Felder vom Browser gefüllt werden
    setTimeout(() => {
      initSavedTabData();  // Tab-Daten initialisieren (verhindert Überschreiben)
    }, 100);
    loadDraftsFromStorage();
    loadTileClipboard();
    loadSensorValues();
    let savedTab = null;
    try { savedTab = localStorage.getItem('activeAdminTab'); } catch (e) {}
    const targetTab = savedTab && document.getElementById(savedTab) ? savedTab : 'tab-tiles-tab0';
    const targetBtn = Array.from(document.querySelectorAll('.tab-btn')).find(btn => btn.getAttribute('onclick')?.includes(targetTab)) || document.querySelector('.tab-btn');
    if (targetBtn) targetBtn.click();
    setInterval(loadSensorValues, 5000);
    enableTileDrag('tab0');
    enableTileDrag('tab1');
    enableTileDrag('tab2');
  });
  </script>
)html";
}
