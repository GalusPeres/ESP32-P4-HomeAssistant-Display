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

  function saveTabName(tabIndex, newName) {
    if (!newName || newName.trim().length === 0) return;
    const trimmedName = newName.trim();

    fetch('/api/tabs/rename', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'tab=' + tabIndex + '&name=' + encodeURIComponent(trimmedName)
    })
    .then(r => r.json())
    .then(data => {
      if (data.success) {
        document.getElementById('tab-name-' + tabIndex).textContent = trimmedName;
        console.log('Tab ' + tabIndex + ' renamed to: ' + trimmedName);
      } else {
        console.error('Error renaming tab:', data.error);
        alert('Fehler beim Umbenennen: ' + (data.error || 'Unbekannt'));
      }
    })
    .catch(e => {
      console.error('Error renaming tab:', e);
      alert('Fehler beim Umbenennen: ' + e);
    });
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
      key_macro: document.getElementById(prefix + '_key_macro')?.value || ''
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
    }
    updateTilePreview(tab);
    return true;
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

  function setupLivePreview(tab) {
    const prefix = tab;
    const fields = [
      '_tile_title','_tile_color','_tile_type','_sensor_entity','_sensor_unit',
      '_sensor_decimals','_scene_alias','_key_macro'
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

    if (titleInput) titleInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (iconInput) iconInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (colorInput) colorInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (typeSelect) typeSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (entitySelect) entitySelect.addEventListener('change', () => { updateTilePreview(tab); updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (unitInput) unitInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (decimalsInput) decimalsInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (sceneInput) sceneInput.addEventListener('input', () => { maybeFillTitleFromScene(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    if (keyInput) keyInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
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

  function updateTilePreview(tab) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const tileId = tab + '-tile-' + currentTileIndex;
    const tileElem = document.getElementById(tileId);
    if (!tileElem) return;

    const wasActive = tileElem.classList.contains('active');
    const typeWas = tileElem.classList.contains('sensor') ? '1' :
                    tileElem.classList.contains('scene')  ? '2' :
                    tileElem.classList.contains('key')    ? '3' : '0';
    const title = document.getElementById(prefix + '_tile_title').value;
    const color = document.getElementById(prefix + '_tile_color').value;
    const type = document.getElementById(prefix + '_tile_type').value;
    const iconInput = document.getElementById(prefix + '_tile_icon');
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
    }

    let html = '';

    // Icon (optional)
    if (iconName) {
      html += '<i class="mdi mdi-' + iconName + ' tile-icon"></i>';
    }

    // Title
    let titleText = title || (type === '1' ? 'Sensor' : type === '2' ? 'Szene' : 'Key');
    html += '<div class="tile-title';
    if (iconName && type === '1') {
      html += ' with-icon';
    }
    html += '" id="' + tileId + '-title">' + titleText + '</div>';

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

  tileElem.innerHTML = html;
  if (wasActive) tileElem.classList.add('active');
  if (typeWas !== type && wasActive) {
    tileElem.classList.add('active');
    const settingsId = tab + 'Settings';
    document.getElementById(settingsId)?.classList.remove('hidden');
  }
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
    ['_sensor_entity','_sensor_unit','_sensor_decimals','_scene_alias','_key_macro'].forEach(suf => {
      const el = document.getElementById(prefix + suf);
      if (el) el.value = '';
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
    if (tile.type === 1) cls.push('sensor'); else if (tile.type === 2) cls.push('scene'); else if (tile.type === 3) cls.push('key'); else cls.push('empty');
    el.className = cls.join(' ');
    if (tile.type === 0) el.style.background = 'transparent';
    else if (tile.type === 1) el.style.background = tile.bg_color ? ('#' + ('000000' + tile.bg_color.toString(16)).slice(-6)) : '#2A2A2A';
    else el.style.background = tile.bg_color ? ('#' + ('000000' + tile.bg_color.toString(16)).slice(-6)) : '#353535';
    if (tile.type === 0) { el.innerHTML = ''; }
    else {
      let title = tile.title && tile.title.length ? tile.title : (tile.type === 1 ? 'Sensor' : tile.type === 2 ? 'Szene' : 'Key');
      let html = '<div class="tile-title" id="' + tab + '-tile-' + index + '-title">' + title + '</div>';
      if (tile.type === 1) {
        let value = '--';
        if (tile.sensor_entity) value = formatSensorValue(sensorValues[tile.sensor_entity] ?? '--', tile.sensor_decimals);
        const unit = tile.sensor_unit || '';
        html += '<div class="tile-value" id="' + tab + '-tile-' + index + '-value">' + value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '') + '</div>';
      }
      el.innerHTML = html;
    }
    if (currentTileTab === tab && currentTileIndex === index) el.classList.add('active');
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
    loadDraftsFromStorage();
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
