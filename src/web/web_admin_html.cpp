#include "src/web/web_admin.h"
#include "src/web/web_admin_utils.h"
#include <WiFi.h>
#include "src/core/config_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/game/game_controls_config.h"
#include "src/tiles/tile_config.h"

String WebAdminServer::getAdminPage() {
  const DeviceConfig& cfg = configManager.getConfig();
  const HaBridgeConfigData& ha = haBridgeConfig.get();
  const auto sensorOptions = parseSensorList(ha.sensors_text);
  const auto sceneOptions = parseSceneList(ha.scene_alias_text);

  auto appendList = [&](String& target, const String& raw) {
    if (!raw.length()) {
      target += "<p class=\"hint\">Keine Eintraege.</p>";
      return;
    }
    target += "<ul class=\"list\">";
    int start = 0;
    while (start < raw.length()) {
      int end = raw.indexOf('\n', start);
      if (end < 0) end = raw.length();
      String line = raw.substring(start, end);
      line.trim();
      if (line.length()) {
        target += "<li>";
        appendHtmlEscaped(target, line);
        target += "</li>";
      }
      start = end + 1;
    }
    target += "</ul>";
  };

  String html;
  html.reserve(12000);
  html += R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Tab5 Admin</title>
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; background:#eef2ff; margin:0; padding:0; }
    .wrapper { max-width:1200px; margin:20px auto; padding:20px; }
    .card { background:#fff; border-radius:16px; box-shadow:0 20px 45px rgba(15,23,42,0.15); padding:32px; }
    h1 { margin:0 0 8px; font-size:28px; color:#1e293b; }
    .subtitle { color:#475569; margin-bottom:24px; }
    .status { display:grid; grid-template-columns:repeat(auto-fit,minmax(220px,1fr)); gap:12px; margin-bottom:28px; }
    .status div { background:#f8fafc; border-radius:12px; padding:14px; border:1px solid #e2e8f0; }
    .status-label { font-size:12px; text-transform:uppercase; color:#64748b; letter-spacing:.08em; }
    .status-value { font-size:16px; color:#0f172a; font-weight:600; }

    /* Tab Navigation */
    .tab-nav { display:flex; gap:8px; margin-bottom:24px; border-bottom:2px solid #e2e8f0; }
    .tab-btn { padding:12px 24px; border:none; background:transparent; color:#64748b; font-size:15px; font-weight:600; cursor:pointer; border-bottom:3px solid transparent; transition:all 0.3s; }
    .tab-btn:hover { color:#4f46e5; background:#f8fafc; }
    .tab-btn.active { color:#4f46e5; border-bottom-color:#4f46e5; }
    .tab-content { display:none; }
    .tab-content.active { display:block; }

    form { display:grid; gap:16px; margin-bottom:32px; }
    label { font-size:13px; font-weight:600; color:#475569; display:block; margin-bottom:6px; }
    input { width:100%; padding:12px; border:1px solid #cbd5f5; border-radius:10px; font-size:15px; box-sizing:border-box; }
    .btn { padding:12px 18px; border:none; border-radius:10px; background:#4f46e5; color:#fff; font-size:16px; cursor:pointer; transition:background 0.2s; }
    .btn:hover { background:#4338ca; }
    .btn-secondary { background:#94a3b8; margin-top:12px; width:100%; }
    .section-title { margin:32px 0 12px; text-transform:uppercase; font-size:12px; letter-spacing:.1em; color:#a1a1aa; }
    .hint { color:#64748b; font-size:14px; margin:8px 0 16px; }
    .list-block { background:#f8fafc; border-radius:12px; padding:16px; border:1px solid #e2e8f0; }
    .list-block strong { display:block; margin:12px 0 6px; color:#1e293b; }
    .list { list-style:none; padding-left:18px; margin:0; }
    .list li { padding:4px 0; font-family:monospace; color:#0f172a; }
    .layout-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:16px; }
    .slot { background:#f8fafc; border:1px solid #e2e8f0; border-radius:12px; padding:12px; }
    .slot-scene { background:#fff7ed; border-color:#fed7aa; }
    .slot-label { font-size:13px; font-weight:600; color:#475569; margin-bottom:8px; }
    .slot select, .slot input { width:100%; box-sizing:border-box; }
    .slot select { padding:10px; border:1px solid #cbd5f5; border-radius:10px; font-size:15px; background:#fff; margin-bottom:8px; }
    .slot input { padding:9px; border:1px solid #d6defa; border-radius:10px; font-size:13px; margin-bottom:6px; }

    /* Tile Editor - M5Stack Tab5: Content 1100x720 (50% Web-Skalierung) */
    /* Original: Tile 335x150px, Gap 24px Ã¢â€ â€™ Web: Tile 168x75px, Gap 12px */
    .tile-editor { display:grid; grid-template-columns:auto 350px; gap:24px; align-items:start; }
    .tile-grid {
      display:grid;
      grid-template-columns:repeat(3, 168px);
      grid-template-rows:repeat(4, 75px);
      gap:12px;
      padding:12px;
      background:#000;
      border-radius:8px;
      width:fit-content;
      height:fit-content;
    }

    /* Display-ÃƒÂ¤hnliche Kacheln (50% Skalierung) */
    /* Display: Title=TOP_LEFT, Value=CENTER(-30,18), Unit=RIGHT_MID */
    .tile {
      background:#2A2A2A;
      border-radius:11px;
      cursor:pointer;
      border:3px solid transparent;
      padding:12px 10px;
      position:relative;
      box-sizing:border-box;
    }
    /* Sensor tiles: Grid-Layout fÃƒÂ¼r Title + Value */
    .tile.sensor {
      display:grid;
      grid-template-rows:auto 1fr;
      grid-template-columns:1fr;
    }
    /* Scene/Key tiles: Flex-Layout fÃƒÂ¼r komplett zentrierten Titel (lv_obj_center) */
    .tile.scene,
    .tile.key {
      display:flex;
      align-items:center;
      justify-content:center;
    }
    .tile.active {
      border:3px solid #4A9EFF;
      box-shadow:0 0 12px rgba(74,158,255,0.6);
    }
    .tile.dragging {
      opacity:0.6;
      border:3px dashed #4A9EFF;
    }
    .tile.drop-target {
       border:3px dashed #4A9EFF;
       background:rgba(74,158,255,0.12);
       box-shadow:0 0 0 2px rgba(74,158,255,0.2) inset;
    }
    .tile.active:hover {
      opacity:1;
      filter:none;
    }
    /* Empty tiles: komplett transparent wie im Display */
      .tile.empty {
        background:transparent !important;
        border:3px solid transparent;
      }
      .tile.empty.active {
        border:3px solid #4A9EFF;
        box-shadow:0 0 12px rgba(74,158,255,0.6);
      }
      .tile.empty:hover:not(.active) {
        border-color:rgba(74,158,255,0.4);
      }
    /* Sensor tiles: Title linksbÃƒÂ¼ndig (TOP_LEFT) */
    .tile-title {
      color:#fff;
      font-weight:normal;
      font-size:12px;
      text-align:left;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
      align-self:start;
    }
    /* Scene/Key tiles: Title komplett zentriert */
    .tile.scene .tile-title,
    .tile.key .tile-title {
      text-align:center;
      align-self:auto;
      width:100%;
    }
    .tile-value {
      color:#fff;
      font-size:24px;
      font-weight:normal;
      text-align:center;
      opacity:0.95;
      line-height:1;
      align-self:center;
      justify-self:center;
      margin-left:-15px;
      margin-top:9px;
    }
    .tile-unit {
      color:#e6e6e6;
      font-size:14px;
      opacity:0.95;
      margin-left:7px;
    }

    /* Settings Panel */
    .tile-settings {
      background:#f8fafc;
      border-radius:12px;
      padding:20px;
      height:fit-content;
      position:sticky;
      top:20px;
    }
    .tile-settings.hidden { display:none; }
    .tile-settings input, .tile-settings select { margin-bottom:12px; }
    .tile-settings h3 { margin:0 0 16px; color:#1e293b; font-size:18px; }
    .type-fields { display:none; margin-top:12px; }
    .type-fields.show { display:block; }

    /* Notification */
    .notification {
      position:fixed;
      bottom:24px;
      right:24px;
      background:#10b981;
      color:#fff;
      padding:16px 24px;
      border-radius:12px;
      box-shadow:0 10px 30px rgba(0,0,0,0.2);
      font-weight:600;
      opacity:0;
      transform:translateY(20px);
      transition:all 0.3s;
      z-index:1000;
    }
    .notification.show {
      opacity:1;
      transform:translateY(0);
    }
  </style>
  <script>
  function switchTab(tabName) {
    // Hide all tabs
    const tabs = document.querySelectorAll('.tab-content');
    tabs.forEach(tab => tab.classList.remove('active'));

    // Remove active class from all buttons
    const btns = document.querySelectorAll('.tab-btn');
    btns.forEach(btn => btn.classList.remove('active'));

    // Show selected tab
    document.getElementById(tabName).classList.add('active');

    // Highlight active button
    event.target.classList.add('active');

    // Persist active tab
    try { localStorage.setItem('activeAdminTab', tabName); } catch (e) {}
  }

    // Tile Editor State
    let currentTileTab = 'home';
    let currentTileIndex = -1;
    let drafts = { home: {}, game: {} };

    function persistDrafts() {
      try { localStorage.setItem('tileDrafts', JSON.stringify(drafts)); } catch (e) {}
    }
    function loadDraftsFromStorage() {
      try {
        const raw = localStorage.getItem('tileDrafts');
        if (raw) drafts = JSON.parse(raw);
      } catch (e) {
        drafts = { home: {}, game: {} };
      }
    }
    function clearDraft(tab, index) {
      if (drafts[tab] && drafts[tab][index]) {
        delete drafts[tab][index];
        persistDrafts();
      }
    }

    function updateDraft(tab) {
      if (currentTileIndex === -1) return;
      const prefix = tab === 'home' ? 'home' : 'game';
      const d = {
        type: document.getElementById(prefix + '_tile_type')?.value || '0',
        title: document.getElementById(prefix + '_tile_title')?.value || '',
        color: document.getElementById(prefix + '_tile_color')?.value || '#2A2A2A',
        sensor_entity: document.getElementById(prefix + '_sensor_entity')?.value || '',
        sensor_unit: document.getElementById(prefix + '_sensor_unit')?.value || '',
        scene_alias: document.getElementById(prefix + '_scene_alias')?.value || '',
        key_macro: document.getElementById(prefix + '_key_macro')?.value || ''
      };
      drafts[tab][currentTileIndex] = d;
      persistDrafts();
    }

    function applyDraft(tab, index) {
      const d = drafts[tab] && drafts[tab][index];
      if (!d) return false;
      const prefix = tab === 'home' ? 'home' : 'game';

      document.getElementById(prefix + '_tile_type').value = d.type || '0';
      updateTileType(tab);

      document.getElementById(prefix + '_tile_title').value = d.title || '';
      document.getElementById(prefix + '_tile_color').value = d.color || '#2A2A2A';

      if (d.type === '1') {
        document.getElementById(prefix + '_sensor_entity').value = d.sensor_entity || '';
        document.getElementById(prefix + '_sensor_unit').value = d.sensor_unit || '';
      } else if (d.type === '2') {
        document.getElementById(prefix + '_scene_alias').value = d.scene_alias || '';
      } else if (d.type === '3') {
        document.getElementById(prefix + '_key_macro').value = d.key_macro || '';
      }

      updateTilePreview(tab);
      return true;
    }

    // Select a tile for editing
    function selectTile(index, tab) {
      currentTileIndex = index;
      currentTileTab = tab;

      // Remove active class from all tiles
      document.querySelectorAll('.tile').forEach(t => t.classList.remove('active'));

      // Add active class to selected tile
      const tileId = tab + '-tile-' + index;
      document.getElementById(tileId).classList.add('active');

      // Show settings panel
      const settingsId = tab === 'home' ? 'homeSettings' : 'gameSettings';
      document.getElementById(settingsId).classList.remove('hidden');

      // Load tile data
      loadTileData(index, tab);

      // Setup live preview listeners
      setupLivePreview(tab);
    }

    // Setup live preview event listeners
    function setupLivePreview(tab) {
      const prefix = tab === 'home' ? 'home' : 'game';

      // Remove old listeners by cloning
      const oldTitle = document.getElementById(prefix + '_tile_title');
      const oldColor = document.getElementById(prefix + '_tile_color');
      const oldType = document.getElementById(prefix + '_tile_type');
      const oldEntity = document.getElementById(prefix + '_sensor_entity');
      const oldUnit = document.getElementById(prefix + '_sensor_unit');
      const oldScene = document.getElementById(prefix + '_scene_alias');
      const oldKey = document.getElementById(prefix + '_key_macro');

      if (oldTitle) oldTitle.replaceWith(oldTitle.cloneNode(true));
      if (oldColor) oldColor.replaceWith(oldColor.cloneNode(true));
      if (oldType) oldType.replaceWith(oldType.cloneNode(true));
      if (oldEntity) oldEntity.replaceWith(oldEntity.cloneNode(true));
      if (oldUnit) oldUnit.replaceWith(oldUnit.cloneNode(true));
      if (oldScene) oldScene.replaceWith(oldScene.cloneNode(true));
      if (oldKey) oldKey.replaceWith(oldKey.cloneNode(true));

      // Add new listeners
      const titleInput = document.getElementById(prefix + '_tile_title');
      const colorInput = document.getElementById(prefix + '_tile_color');
      const typeSelect = document.getElementById(prefix + '_tile_type');
      const entitySelect = document.getElementById(prefix + '_sensor_entity');
      const unitInput = document.getElementById(prefix + '_sensor_unit');
      const sceneInput = document.getElementById(prefix + '_scene_alias');
      const keyInput = document.getElementById(prefix + '_key_macro');

      if (titleInput) {
        titleInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); });
      }
      if (colorInput) {
        colorInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); });
      }
      if (typeSelect) {
        typeSelect.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); });
      }
      if (entitySelect) {
        entitySelect.addEventListener('change', () => {
          updateTilePreview(tab);
          updateSensorValuePreview(tab);
          updateDraft(tab);
        });
      }
      if (unitInput) {
        unitInput.addEventListener('input', () => { updateSensorValuePreview(tab); updateDraft(tab); });
      }
      if (sceneInput) {
        sceneInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); });
      }
      if (keyInput) {
        keyInput.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); });
      }
    }

    // Update sensor value in preview when entity is selected
    function updateSensorValuePreview(tab) {
      if (currentTileIndex === -1) return;

      const prefix = tab === 'home' ? 'home' : 'game';
      const entitySelect = document.getElementById(prefix + '_sensor_entity');
      const unitInput = document.getElementById(prefix + '_sensor_unit');

      if (!entitySelect) return;

      const entity = entitySelect.value;
      if (!entity) {
        // No entity selected - show placeholder
        const valueElem = document.getElementById(tab + '-tile-' + currentTileIndex + '-value');
        if (valueElem) {
          const unit = unitInput ? unitInput.value : '';
          valueElem.innerHTML = '--' + (unit ? '<span class="tile-unit">' + unit + '</span>' : '');
        }
        return;
      }

      // Fetch current sensor values
      fetch('/api/sensor_values')
        .then(res => res.json())
        .then(values => {
          const valueElem = document.getElementById(tab + '-tile-' + currentTileIndex + '-value');
          if (valueElem) {
            let value = values[entity] || '--';
            if (value.toLowerCase() === 'unavailable') value = '--';
            const unit = unitInput ? unitInput.value : '';
            valueElem.innerHTML = value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '');
            console.log('Preview Update:', entity, '=', value, unit);
          }
        })
        .catch(err => console.error('Fehler beim Laden des Sensorwerts:', err));
    }

    // Update tile preview in real-time
    function updateTilePreview(tab) {
      if (currentTileIndex === -1) return;

      const prefix = tab === 'home' ? 'home' : 'game';
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

      // Update CSS classes and background wie im Display
      tileElem.className = 'tile';  // Reset classes
      tileElem.style.background = '';

      if (type === '0') {
        // EMPTY: transparent, keine Klasse
        tileElem.classList.add('empty');
        tileElem.innerHTML = '';  // Kein Content fÃƒÂ¼r leere Tiles!
        if (wasActive) tileElem.classList.add('active');
        return;
      } else if (type === '1') {
        // SENSOR
        tileElem.classList.add('sensor');
        tileElem.style.background = color || '#2A2A2A';
      } else if (type === '2') {
        // SCENE
        tileElem.classList.add('scene');
        tileElem.style.background = color || '#353535';
      } else if (type === '3') {
        // KEY
        tileElem.classList.add('key');
        tileElem.style.background = color || '#353535';
      }

      // Rebuild tile content based on type
      let titleText = title || (type === '1' ? 'Sensor' : type === '2' ? 'Szene' : 'Key');
      let html = '<div class="tile-title" id="' + tileId + '-title">' + titleText + '</div>';

      // Add value element for sensors
      if (type === '1') {
        const entitySelect = document.getElementById(prefix + '_sensor_entity');
        const unitInput = document.getElementById(prefix + '_sensor_unit');
        const entity = entitySelect ? entitySelect.value : '';
        const unit = unitInput ? unitInput.value : '';

        html += '<div class="tile-value" id="' + tileId + '-value">--';
        if (unit) {
          html += '<span class="tile-unit">' + unit + '</span>';
        }
        html += '</div>';

        // Fetch value if entity is selected
        if (entity) {
          tileElem.innerHTML = html;
          if (wasActive) tileElem.classList.add('active');
          updateSensorValuePreview(tab);
          return;
        }
      }

      tileElem.innerHTML = html;
      if (wasActive) tileElem.classList.add('active');

      // Preserve selection when type changes from empty -> scene/key/etc.
      if (typeWas !== type && wasActive) {
        tileElem.classList.add('active');
        const settingsId = tab === 'home' ? 'homeSettings' : 'gameSettings';
        document.getElementById(settingsId)?.classList.remove('hidden');
      }
    }

    // Load tile data from server
    function loadTileData(index, tab) {
      fetch('/api/tiles?tab=' + tab + '&index=' + index)
        .then(res => res.json())
        .then(data => {
          const prefix = tab === 'home' ? 'home' : 'game';

          // Set type
          document.getElementById(prefix + '_tile_type').value = data.type || 0;
          updateTileType(tab);

          // Set common fields
          document.getElementById(prefix + '_tile_title').value = data.title || '';
          document.getElementById(prefix + '_tile_color').value = rgbToHex(data.bg_color || 0x2A2A2A);

          // Set type-specific fields
          if (data.type === 1) { // Sensor
            document.getElementById(prefix + '_sensor_entity').value = data.sensor_entity || '';
            document.getElementById(prefix + '_sensor_unit').value = data.sensor_unit || '';
          } else if (data.type === 2) { // Scene
            document.getElementById(prefix + '_scene_alias').value = data.scene_alias || '';
          } else if (data.type === 3) { // Key
            document.getElementById(prefix + '_key_macro').value = data.key_macro || '';
          }

          // Ensure selection stays highlighted after async load
          const tileElem = document.getElementById(tab + '-tile-' + index);
          if (tileElem) tileElem.classList.add('active');

          // Apply unsaved draft if present
          if (!applyDraft(tab, index)) {
            updateTilePreview(tab);
          }
        });
    }

    // Update tile type visibility
    function updateTileType(tab) {
      const prefix = tab === 'home' ? 'home' : 'game';
      const typeValue = document.getElementById(prefix + '_tile_type').value;

      // Hide all type-specific fields
      document.querySelectorAll('#' + prefix + 'Settings .type-fields').forEach(f => f.classList.remove('show'));

      // Show relevant fields
      if (typeValue === '1') {
        document.getElementById(prefix + '_sensor_fields').classList.add('show');
      } else if (typeValue === '2') {
        document.getElementById(prefix + '_scene_fields').classList.add('show');
      } else if (typeValue === '3') {
        document.getElementById(prefix + '_key_fields').classList.add('show');
      }
    }

    // Show notification
    function showNotification(message, success = true) {
      const notification = document.getElementById('notification');
      notification.textContent = message;
      notification.style.background = success ? '#10b981' : '#ef4444';
      notification.classList.add('show');

      setTimeout(() => {
        notification.classList.remove('show');
      }, 3000);
    }

    // Save tile configuration
    function saveTile(tab) {
      if (currentTileIndex === -1) return;

      const prefix = tab === 'home' ? 'home' : 'game';
      const formData = new FormData();

      formData.append('tab', tab);
      formData.append('index', currentTileIndex);
      formData.append('type', document.getElementById(prefix + '_tile_type').value);
      formData.append('title', document.getElementById(prefix + '_tile_title').value);
      formData.append('bg_color', hexToRgb(document.getElementById(prefix + '_tile_color').value));

      const typeValue = document.getElementById(prefix + '_tile_type').value;
      if (typeValue === '1') {
        formData.append('sensor_entity', document.getElementById(prefix + '_sensor_entity').value);
        formData.append('sensor_unit', document.getElementById(prefix + '_sensor_unit').value);
      } else if (typeValue === '2') {
        formData.append('scene_alias', document.getElementById(prefix + '_scene_alias').value);
      } else if (typeValue === '3') {
        formData.append('key_macro', document.getElementById(prefix + '_key_macro').value);
      }

      fetch('/api/tiles', {
        method: 'POST',
        body: formData
      })
      .then(res => res.json())
      .then(data => {
        if (data.success) {
          console.log('Kachel gespeichert:', {tab, index: currentTileIndex});
          showNotification('Kachel gespeichert & Display aktualisiert!');
          clearDraft(tab, currentTileIndex);
        } else {
          console.error('Save failed:', data.error);
          showNotification('Fehler: ' + (data.error || 'Unbekannt'), false);
        }
      })
      .catch(err => {
        console.error('Network error:', err);
        showNotification('Netzwerkfehler beim Speichern', false);
      });
    }

    // Color conversion helpers
    function rgbToHex(rgb) {
      return '#' + ('000000' + rgb.toString(16)).slice(-6);
    }

    function hexToRgb(hex) {
      return parseInt(hex.replace('#', ''), 16);
    }

    // Load and update sensor values - efficient version
    function loadSensorValues() {
      // Fetch all data in parallel: sensor values + tile configs
      Promise.all([
        fetch('/api/sensor_values').then(res => res.json()),
        fetch('/api/tiles?tab=home').then(res => res.json()),
        fetch('/api/tiles?tab=game').then(res => res.json())
      ])
      .then(([sensorValues, homeTiles, gameTiles]) => {
        console.log('Sensorwerte geladen:', sensorValues);
        console.log('Home Tiles:', homeTiles);
        console.log('Game Tiles:', gameTiles);

        // Update home tiles
        homeTiles.forEach((tile, index) => {
          if (tile.type === 1 && tile.sensor_entity) {
            const valueElem = document.getElementById('home-tile-' + index + '-value');
            if (valueElem) {
              let value = sensorValues[tile.sensor_entity] || '--';
              if (value.toLowerCase() === 'unavailable') value = '--';
              const unit = tile.sensor_unit || '';
              valueElem.innerHTML = value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '');
              console.log('Home Tile', index, ':', tile.sensor_entity, '=', value);
            }
          }
        });

        // Update game tiles
        gameTiles.forEach((tile, index) => {
          if (tile.type === 1 && tile.sensor_entity) {
            const valueElem = document.getElementById('game-tile-' + index + '-value');
            if (valueElem) {
              let value = sensorValues[tile.sensor_entity] || '--';
              if (value.toLowerCase() === 'unavailable') value = '--';
              const unit = tile.sensor_unit || '';
              valueElem.innerHTML = value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '');
              console.log('Game Tile', index, ':', tile.sensor_entity, '=', value);
            }
          }
        });
      })
      .catch(err => console.error('Fehler beim Laden der Sensorwerte:', err));

      // Re-apply active highlighting + settings panel for current selection
      if (currentTileIndex !== -1) {
        const tab = currentTileTab === 'game' ? 'game' : 'home';
        const settingsId = tab === 'home' ? 'homeSettings' : 'gameSettings';
        document.getElementById(settingsId)?.classList.remove('hidden');
        document.querySelectorAll('.tile').forEach(t => t.classList.remove('active'));
        const activeTile = document.getElementById(tab + '-tile-' + currentTileIndex);
        if (activeTile) activeTile.classList.add('active');
      }
    }

    // Drag & Drop Swap (Tile reorder)
    let dragSource = null;

    function enableTileDrag(tab) {
      const tiles = document.querySelectorAll('#tab-tiles-' + tab + ' .tile');
      tiles.forEach(tile => {
        tile.addEventListener('dragstart', (e) => {
          dragSource = { tab, index: parseInt(tile.dataset.index) };
          e.dataTransfer.effectAllowed = 'move';
          tile.classList.add('dragging');
          if (e.dataTransfer.setDragImage) {
            e.dataTransfer.setDragImage(tile, tile.clientWidth / 2, tile.clientHeight / 2);
          }
        });
        tile.addEventListener('dragend', () => {
          tile.classList.remove('dragging');
          tiles.forEach(t => t.classList.remove('drop-target'));
          dragSource = null;
        });
        tile.addEventListener('dragenter', (e) => {
          e.preventDefault();
          tile.classList.add('drop-target');
        });
        tile.addEventListener('dragover', (e) => {
          e.preventDefault();
          tile.classList.add('drop-target');
        });
        tile.addEventListener('dragleave', () => {
          tile.classList.remove('drop-target');
        });
        tile.addEventListener('drop', (e) => {
          e.preventDefault();
          tile.classList.remove('drop-target');
          if (!dragSource || dragSource.tab !== tab) return;
          const targetIndex = parseInt(tile.dataset.index);
          if (isNaN(targetIndex) || targetIndex === dragSource.index) return;
          swapTilesOnServer(tab, dragSource.index, targetIndex);
        });
      });
    }

    function applyDomSwap(tab, fromIndex, toIndex) {
      const grid = document.querySelector('#tab-tiles-' + tab + ' .tile-grid');
      if (!grid) return;
      const tiles = Array.from(grid.children);
      const a = tiles[fromIndex];
      const b = tiles[toIndex];
      if (!a || !b) return;
      // Swap nodes
      const sibling = b.nextSibling;
      grid.insertBefore(b, a);
      grid.insertBefore(a, sibling);
      // Reindex ids/datasets to keep previews/updates consistent
      Array.from(grid.children).forEach((el, idx) => {
        el.dataset.index = idx.toString();
        el.id = tab + '-tile-' + idx;
        const title = el.querySelector('.tile-title');
        if (title) title.id = tab + '-tile-' + idx + '-title';
        const value = el.querySelector('.tile-value');
        if (value) value.id = tab + '-tile-' + idx + '-value';
      });
    }

    function swapTilesOnServer(tab, fromIndex, toIndex) {
      const fd = new FormData();
      fd.append('tab', tab);
      fd.append('from', fromIndex);
      fd.append('to', toIndex);
      fetch('/api/tiles/reorder', {
        method: 'POST',
        body: fd
      }).then(res => {
        if (!res.ok) throw new Error('http');
        return res.json();
      }).then(() => {
        // Lokale Arrays und DOM tauschen, dann Sensorwerte neu laden
        try {
          if (tab === 'home' && Array.isArray(homeTiles)) {
            [homeTiles[fromIndex], homeTiles[toIndex]] = [homeTiles[toIndex], homeTiles[fromIndex]];
          } else if (tab === 'game' && Array.isArray(gameTiles)) {
            [gameTiles[fromIndex], gameTiles[toIndex]] = [gameTiles[toIndex], gameTiles[fromIndex]];
          }
        } catch (e) {}
        applyDomSwap(tab, fromIndex, toIndex);
        loadSensorValues();
        showNotification('Reihenfolge gespeichert');
      }).catch(() => {
        showNotification('Tausch fehlgeschlagen', true);
      });
    }

    window.onload = function() {
      loadDraftsFromStorage();
      // Activate saved tab or default to Tiles Home
      let savedTab = null;
      try { savedTab = localStorage.getItem('activeAdminTab'); } catch (e) {}
      const targetTab = savedTab && document.getElementById(savedTab) ? savedTab : 'tab-tiles-home';
      const targetBtn = Array.from(document.querySelectorAll('.tab-btn')).find(btn => btn.getAttribute('onclick')?.includes(targetTab)) || document.querySelector('.tab-btn');
      if (targetBtn) targetBtn.click();

      // Load sensor values
      loadSensorValues();

      // Refresh sensor values every 5 seconds
      setInterval(loadSensorValues, 5000);

      // Enable drag & drop for both grids
      enableTileDrag('home');
      enableTileDrag('game');
    };
  </script>
</head>
<body>
  <div class="wrapper">
    <div class="card">
      <h1>Tab5 Admin-Panel</h1>
      <p class="subtitle">Konfiguration &amp; Uebersicht</p>

      <!-- WiFi Status at top -->
      <div class="status">
        <div>
          <div class="status-label">WiFi Status</div>
          <div class="status-value">)html";
  html += (WiFi.status() == WL_CONNECTED) ? "Verbunden" : "Getrennt";
  html += R"html(</div>
        </div>
        <div>
          <div class="status-label">SSID</div>
          <div class="status-value">)html";
  html += WiFi.SSID();
  html += R"html(</div>
        </div>
        <div>
          <div class="status-label">IP-Adresse</div>
          <div class="status-value">)html";
  html += WiFi.localIP().toString();
  html += R"html(</div>
        </div>
      </div>

      <!-- Tab Navigation -->
      <div class="tab-nav">
        <button class="tab-btn" onclick="switchTab('tab-network')">Network</button>
        <button class="tab-btn" onclick="switchTab('tab-tiles-home')">Tiles Home</button>
        <button class="tab-btn" onclick="switchTab('tab-tiles-game')">Tiles Game</button>
        <button class="tab-btn" onclick="switchTab('tab-home')">Home (Alt)</button>
        <button class="tab-btn" onclick="switchTab('tab-game')">Game (Alt)</button>
      </div>

      <!-- Tab 1: Network (MQTT Configuration) -->
      <div id="tab-network" class="tab-content">
        <form action="/mqtt" method="POST">
          <div>
            <label for="mqtt_host">MQTT Host / IP</label>
            <input type="text" id="mqtt_host" name="mqtt_host" required value=")html";
  html += cfg.mqtt_host;
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_port">Port</label>
            <input type="number" id="mqtt_port" name="mqtt_port" value=")html";
  html += String(cfg.mqtt_port ? cfg.mqtt_port : 1883);
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_user">Benutzername</label>
            <input type="text" id="mqtt_user" name="mqtt_user" value=")html";
  html += cfg.mqtt_user;
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_pass">Passwort</label>
            <input type="password" id="mqtt_pass" name="mqtt_pass" value=")html";
  html += cfg.mqtt_pass;
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_base">Ger&auml;te-Topic Basis</label>
            <input type="text" id="mqtt_base" name="mqtt_base" value=")html";
  html += cfg.mqtt_base_topic;
  html += R"html(">
          </div>
          <div>
            <label for="ha_prefix">Home Assistant Prefix</label>
            <input type="text" id="ha_prefix" name="ha_prefix" value=")html";
  html += cfg.ha_prefix;
  html += R"html(">
          </div>
          <button class="btn" type="submit">Speichern</button>
        </form>
      </div>

      <!-- Tab 2: Home (6 Sensors + 6 Scenes) -->
      <div id="tab-home" class="tab-content">
        <p class="hint">Ordne hier die 3x4 Kacheln zu. Die oberen zwei Reihen zeigen Sensoren, die unteren zwei Reihen Szenen. Auswahl &quot;Keine&quot; blendet eine Kachel aus.</p>
        <form action="/bridge" method="POST">
          <div class="layout-grid">
)html";

  auto appendSlot = [&](bool sensor, size_t index, const String& current) {
    const char* labels_sensor[] = {
      "Sensor 1 (oben links)",
      "Sensor 2",
      "Sensor 3",
      "Sensor 4",
      "Sensor 5",
      "Sensor 6"
    };
    const char* labels_scene[] = {
      "Szene 1",
      "Szene 2",
      "Szene 3",
      "Szene 4",
      "Szene 5",
      "Szene 6"
    };
    String field = sensor ? "sensor_slot" : "scene_slot";
    field += static_cast<int>(index);
    html += "<div class=\"slot ";
    html += sensor ? "slot-sensor" : "slot-scene";
    html += "\"><div class=\"slot-label\">";
    html += sensor ? labels_sensor[index] : labels_scene[index];
    html += "</div><select name=\"";
    html += field;
    html += "\"><option value=\"\"";
    if (!current.length()) html += " selected";
    html += ">Keine</option>";

    if (sensor) {
      for (const auto& opt : sensorOptions) {
        bool selected = current.equalsIgnoreCase(opt);
        html += "<option value=\"";
        appendHtmlEscaped(html, opt);
        html += "\"";
        if (selected) html += " selected";
        html += ">";
        String label = humanizeIdentifier(opt, true) + " - " + opt;
        appendHtmlEscaped(html, label);
        html += "</option>";
      }
    } else {
      for (const auto& opt : sceneOptions) {
        bool selected = current.equalsIgnoreCase(opt.alias);
        html += "<option value=\"";
        appendHtmlEscaped(html, opt.alias);
        html += "\"";
        if (selected) html += " selected";
        html += ">";
        String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
        appendHtmlEscaped(html, label);
        html += "</option>";
      }
    }
    html += "</select>";

    String custom_value = sensor ? ha.sensor_titles[index] : ha.scene_titles[index];
    String placeholder;
    if (sensor && current.length()) {
      placeholder = lookupKeyValue(ha.sensor_names_map, current);
      if (!placeholder.length()) {
        placeholder = humanizeIdentifier(current, true);
      }
    } else if (!sensor && current.length()) {
      placeholder = humanizeIdentifier(current, false);
    }
    String input_name = sensor ? "sensor_label" : "scene_label";
    input_name += static_cast<int>(index);
      html += "<input type=\"text\" name=\"";
      html += input_name;
      html += "\" placeholder=\"";
      if (placeholder.length()) {
        appendHtmlEscaped(html, String("Standard: ") + placeholder);
      } else {
      html += "Eigener Titel";
    }
    html += "\" value=\"";
    appendHtmlEscaped(html, custom_value);
    html += "\">";

    if (sensor) {
      String unit_input = "sensor_unit";
      unit_input += static_cast<int>(index);
      String unit_value = ha.sensor_custom_units[index];
      html += "<input type=\"text\" name=\"";
      html += unit_input;
      html += "\" maxlength=\"10\" placeholder=\"Einheit z.B. &deg;C\" value=\"";
      appendHtmlEscaped(html, unit_value);
      html += "\">";
    }

    // Color Picker
    html += "<div style=\"margin-top:12px;\"><label style=\"font-size:12px;color:#64748b;margin-bottom:4px;display:block;\">Farbe:</label><input type=\"color\" name=\"";
    html += sensor ? "sensor_color" : "scene_color";
    html += String((int)index);
    html += "\" value=\"#";

    // Aktuelle Farbe anzeigen (oder Standard)
    uint32_t current_color = sensor ? ha.sensor_colors[index] : ha.scene_colors[index];
    if (current_color == 0) {
      current_color = sensor ? 0x2A2A2A : 0x353535;  // Standard-Farben
    }
    char colorHex[7];
    snprintf(colorHex, sizeof(colorHex), "%06X", (unsigned int)current_color);
    html += colorHex;
    html += "\" style=\"width:100%;height:40px;cursor:pointer;\"></div>";

    html += "</div>";
  };

  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    appendSlot(true, i, ha.sensor_slots[i]);
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    appendSlot(false, i, ha.scene_slots[i]);
  }

  html += R"html(
          </div>
          <button class="btn" type="submit">Layout speichern</button>
        </form>

        <div class="section-title">Home Assistant Bridge</div>
        <p class="hint">Konfiguration erfolgt in Home Assistant - diese Liste dient nur zur Anzeige.</p>
        <div class="list-block">
          <strong>Sensoren</strong>)html";
  appendList(html, ha.sensors_text);
  html += R"html(
          <strong>Szenen</strong>)html";
  appendList(html, ha.scene_alias_text);
  html += R"html(
        </div>
      </div>

      <!-- Tab 3: Game Controls (12 Buttons) -->
      <div id="tab-game" class="tab-content">
        <p class="hint">Konfiguriere 12 Buttons fÃƒÂ¼r USB-Tastatur-Makros (z.B. Star Citizen). GerÃƒÂ¤t muss per USB am PC angeschlossen sein.</p>
        <form action="/game_controls" method="POST">
          <div class="layout-grid">
)html";

  // Game Controls - 12 Buttons
  const GameControlsConfigData& gameData = gameControlsConfig.get();
  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    html += "<div class=\"slot\" style=\"background:#f0fdf4;border-color:#bbf7d0;\"><div class=\"slot-label\">Button ";
    html += String((int)i + 1);
    html += "</div><input type=\"text\" name=\"game_name";
    html += String((int)i);
    html += "\" placeholder=\"z.B. Landing Gear\" value=\"";
    appendHtmlEscaped(html, gameData.buttons[i].name);
    html += "\" style=\"margin-bottom:8px;\"><input type=\"text\" name=\"game_macro";
    html += String((int)i);
    html += "\" placeholder=\"z.B. g oder ctrl+g oder ctrl+shift+a\" value=\"";

    // Aktuelles Makro anzeigen (aus key_code + modifier rekonstruieren)
    String currentMacro = "";
    if (gameData.buttons[i].key_code != 0) {
      // Modifier hinzufÃƒÂ¼gen
      if (gameData.buttons[i].modifier & 0x01) currentMacro += "ctrl+";
      if (gameData.buttons[i].modifier & 0x02) currentMacro += "shift+";
      if (gameData.buttons[i].modifier & 0x04) currentMacro += "alt+";

      // Taste hinzufÃƒÂ¼gen (Scancode zu Buchstabe konvertieren)
      uint8_t code = gameData.buttons[i].key_code;
      if (code >= 0x04 && code <= 0x1D) currentMacro += (char)('a' + (code - 0x04));
      else if (code >= 0x1E && code <= 0x27) currentMacro += (char)('1' + (code - 0x1E));
      else if (code == 0x2C) currentMacro += "space";
      else if (code == 0x28) currentMacro += "enter";
      else if (code == 0x2A) currentMacro += "backspace";
      else if (code == 0x2B) currentMacro += "tab";
      else if (code == 0x29) currentMacro += "esc";
      else currentMacro += "?";
    }

    appendHtmlEscaped(html, currentMacro);
    html += "\"><div style=\"margin-top:6px;font-size:11px;color:#64748b;\">Beispiele: g, ctrl+g, ctrl+shift+a, space, enter</div>";

    // Color Picker
    html += "<div style=\"margin-top:12px;\"><label style=\"font-size:12px;color:#64748b;margin-bottom:4px;display:block;\">Farbe:</label><input type=\"color\" name=\"game_color";
    html += String((int)i);
    html += "\" value=\"#";

    // Aktuelle Farbe anzeigen (oder Standard)
    uint32_t btn_color = (gameData.buttons[i].color != 0) ? gameData.buttons[i].color : 0x353535;
    char colorHex[7];
    snprintf(colorHex, sizeof(colorHex), "%06X", (unsigned int)btn_color);
    html += colorHex;
    html += "\" style=\"width:100%;height:40px;cursor:pointer;\"></div></div>";
  }

  html += R"html(
          </div>
          <button class="btn" type="submit">Game Controls speichern</button>
        </form>
      </div>

      <!-- Tab 4: Tiles Home Editor -->
      <div id="tab-tiles-home" class="tab-content">
        <p class="hint">Klicke auf eine Kachel, um sie zu bearbeiten. WÃƒÂ¤hle den Typ (Sensor/Szene/Key) und passe die Einstellungen an.</p>
        <div class="tile-editor">
          <!-- Grid Preview -->
          <div class="tile-grid">
)html";

  // Generate 12 tiles for Home
  for (int i = 0; i < 12; i++) {
    const Tile& tile = tileConfig.getHomeGrid().tiles[i];

    // CSS-Klassen und Farben wie im Display
    String cssClass = "tile";
    String tileStyle = "";

    if (tile.type == TILE_EMPTY) {
      cssClass += " empty";
      // Empty: kein background (transparent)
    } else if (tile.type == TILE_SENSOR) {
      cssClass += " sensor";
      // Sensor: Standard 0x2A2A2A
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#2A2A2A";
      }
    } else if (tile.type == TILE_SCENE) {
      cssClass += " scene";
      // Scene: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    } else if (tile.type == TILE_KEY) {
      cssClass += " key";
      // Key: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    }

    html += "<div class=\"";
    html += cssClass;
    html += "\" data-index=\"";
    html += String(i);
    html += "\" draggable=\"true\" id=\"home-tile-";
    html += String(i);
    html += "\" style=\"";
    html += tileStyle;
    html += "\" onclick=\"selectTile(";
    html += String(i);
    html += ", 'home')\">";

    // Title - nur wenn nicht EMPTY
    if (tile.type != TILE_EMPTY) {
      html += "<div class=\"tile-title\" id=\"home-tile-";
      html += String(i);
      html += "-title\">";
      if (tile.title.length()) {
        appendHtmlEscaped(html, tile.title);
      } else if (tile.type == TILE_SENSOR) {
        html += "Sensor";
      } else if (tile.type == TILE_SCENE) {
        html += "Szene";
      } else if (tile.type == TILE_KEY) {
        html += "Key";
      }
      html += "</div>";
    }

    // Value (for sensors)
    if (tile.type == TILE_SENSOR) {
      html += "<div class=\"tile-value\" id=\"home-tile-";
      html += String(i);
      html += "-value\">";

      // Get actual sensor value from map
      String sensorValue = "--";
      if (tile.sensor_entity.length()) {
        sensorValue = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
        Serial.printf("[WebAdmin] Home Tile %d: Entity=%s, Value=%s\n",
                      i, tile.sensor_entity.c_str(),
                      sensorValue.length() ? sensorValue.c_str() : "(empty)");
        if (sensorValue.length() == 0) {
          sensorValue = "--";
        }
      }
      appendHtmlEscaped(html, sensorValue);

      if (tile.sensor_unit.length()) {
        html += "<span class=\"tile-unit\">";
        appendHtmlEscaped(html, tile.sensor_unit);
        html += "</span>";
      }
      html += "</div>";
    }

    html += "</div>";
  }

  html += R"html(
          </div>

          <!-- Settings Panel -->
          <div class="tile-settings hidden" id="homeSettings">
            <h3 style="margin-top:0;">Kachel Einstellungen</h3>

            <label>Typ</label>
            <select id="home_tile_type" onchange="updateTileType('home')">
              <option value="0">Leer</option>
              <option value="1">Sensor</option>
              <option value="2">Szene</option>
              <option value="3">Key</option>
            </select>

            <label>Titel</label>
            <input type="text" id="home_tile_title" placeholder="Kachel-Titel">

            <label>Farbe</label>
            <input type="color" id="home_tile_color" value="#2A2A2A" style="height:40px;">

            <!-- Sensor Fields -->
            <div id="home_sensor_fields" class="type-fields">
              <label>Sensor Entity</label>
              <select id="home_sensor_entity">
                <option value="">Keine Auswahl</option>
)html";

  // Add sensor options
  for (const auto& opt : sensorOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Einheit</label>
              <input type="text" id="home_sensor_unit" placeholder="z.B. Ã‚Â°C">
            </div>

            <!-- Scene Fields -->
            <div id="home_scene_fields" class="type-fields">
              <label>Szene</label>
              <select id="home_scene_alias">
                <option value="">Keine Auswahl</option>
)html";

  // Add scene options
  for (const auto& opt : sceneOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt.alias);
    html += "\">";
    String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
            </div>

            <!-- Key Fields -->
            <div id="home_key_fields" class="type-fields">
              <label>Makro</label>
              <input type="text" id="home_key_macro" placeholder="z.B. ctrl+g">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Beispiele: g, ctrl+g, ctrl+shift+a</div>
            </div>

            <button class="btn" onclick="saveTile('home')" style="margin-top:16px;width:100%;">Speichern</button>
          </div>
        </div>
      </div>

      <!-- Tab 5: Tiles Game Editor -->
      <div id="tab-tiles-game" class="tab-content">
        <p class="hint">Klicke auf eine Kachel, um sie zu bearbeiten. WÃƒÂ¤hle den Typ (Sensor/Szene/Key) und passe die Einstellungen an.</p>
        <div class="tile-editor">
          <!-- Grid Preview -->
          <div class="tile-grid">
)html";

  // Generate 12 tiles for Game
  for (int i = 0; i < 12; i++) {
    const Tile& tile = tileConfig.getGameGrid().tiles[i];

    // CSS-Klassen und Farben wie im Display
    String cssClass = "tile";
    String tileStyle = "";

    if (tile.type == TILE_EMPTY) {
      cssClass += " empty";
      // Empty: kein background (transparent)
    } else if (tile.type == TILE_SENSOR) {
      cssClass += " sensor";
      // Sensor: Standard 0x2A2A2A
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#2A2A2A";
      }
    } else if (tile.type == TILE_SCENE) {
      cssClass += " scene";
      // Scene: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    } else if (tile.type == TILE_KEY) {
      cssClass += " key";
      // Key: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    }

    html += "<div class=\"";
    html += cssClass;
    html += "\" data-index=\"";
    html += String(i);
    html += "\" draggable=\"true\" id=\"game-tile-";
    html += String(i);
    html += "\" style=\"";
    html += tileStyle;
    html += "\" onclick=\"selectTile(";
    html += String(i);
    html += ", 'game')\">";

    // Title - nur wenn nicht EMPTY
    if (tile.type != TILE_EMPTY) {
      html += "<div class=\"tile-title\" id=\"game-tile-";
      html += String(i);
      html += "-title\">";
      if (tile.title.length()) {
        appendHtmlEscaped(html, tile.title);
      } else if (tile.type == TILE_SENSOR) {
        html += "Sensor";
      } else if (tile.type == TILE_SCENE) {
        html += "Szene";
      } else if (tile.type == TILE_KEY) {
        html += "Key";
      }
      html += "</div>";
    }

    // Value (for sensors)
    if (tile.type == TILE_SENSOR) {
      html += "<div class=\"tile-value\" id=\"game-tile-";
      html += String(i);
      html += "-value\">";

      // Get actual sensor value from map
      String sensorValue = "--";
      if (tile.sensor_entity.length()) {
        sensorValue = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
        Serial.printf("[WebAdmin] Game Tile %d: Entity=%s, Value=%s\n",
                      i, tile.sensor_entity.c_str(),
                      sensorValue.length() ? sensorValue.c_str() : "(empty)");
        if (sensorValue.length() == 0) {
          sensorValue = "--";
        }
      }
      appendHtmlEscaped(html, sensorValue);

      if (tile.sensor_unit.length()) {
        html += "<span class=\"tile-unit\">";
        appendHtmlEscaped(html, tile.sensor_unit);
        html += "</span>";
      }
      html += "</div>";
    }

    html += "</div>";
  }

  html += R"html(
          </div>

          <!-- Settings Panel -->
          <div class="tile-settings hidden" id="gameSettings">
            <h3 style="margin-top:0;">Kachel Einstellungen</h3>

            <label>Typ</label>
            <select id="game_tile_type" onchange="updateTileType('game')">
              <option value="0">Leer</option>
              <option value="1">Sensor</option>
              <option value="2">Szene</option>
              <option value="3">Key</option>
            </select>

            <label>Titel</label>
            <input type="text" id="game_tile_title" placeholder="Kachel-Titel">

            <label>Farbe</label>
            <input type="color" id="game_tile_color" value="#2A2A2A" style="height:40px;">

            <!-- Sensor Fields -->
            <div id="game_sensor_fields" class="type-fields">
              <label>Sensor Entity</label>
              <select id="game_sensor_entity">
                <option value="">Keine Auswahl</option>
)html";

  // Add sensor options for game
  for (const auto& opt : sensorOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Einheit</label>
              <input type="text" id="game_sensor_unit" placeholder="z.B. Ã‚Â°C">
            </div>

            <!-- Scene Fields -->
            <div id="game_scene_fields" class="type-fields">
              <label>Szene</label>
              <select id="game_scene_alias">
                <option value="">Keine Auswahl</option>
)html";

  // Add scene options for game
  for (const auto& opt : sceneOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt.alias);
    html += "\">";
    String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
            </div>

            <!-- Key Fields -->
            <div id="game_key_fields" class="type-fields">
              <label>Makro</label>
              <input type="text" id="game_key_macro" placeholder="z.B. ctrl+g">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Beispiele: g, ctrl+g, ctrl+shift+a</div>
            </div>

            <button class="btn" onclick="saveTile('game')" style="margin-top:16px;width:100%;">Speichern</button>
          </div>
        </div>
      </div>

      <!-- Restart button at bottom (always visible) -->
      <form action="/restart" method="POST" onsubmit="return confirm('Geraet wirklich neu starten?');" style="margin-top:32px;">
        <button class="btn btn-secondary" type="submit">Geraet neu starten</button>
      </form>
    </div>
  </div>

  <!-- Notification Toast -->
  <div id="notification" class="notification"></div>
</body>
</html>
)html";

  return html;
}

String WebAdminServer::getSuccessPage() {
  return R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <title>Gespeichert</title>
  <style>
    body { font-family: Arial, sans-serif; background:#eef2ff; height:100vh; margin:0; display:flex; align-items:center; justify-content:center; }
    .box { background:#fff; padding:30px; border-radius:12px; box-shadow:0 15px 35px rgba(0,0,0,.2); text-align:center; }
    h1 { margin:0 0 10px; color:#1f2937; }
    p { margin:0; color:#4b5563; }
  </style>
  <script>setTimeout(function(){window.location.href='/'},1500);</script>
</head>
<body>
  <div class="box">
    <h1>MQTT-Konfiguration gespeichert</h1>
    <p>Das Geraet verbindet sich neu ...</p>
  </div>
</body>
</html>)html";
}

String WebAdminServer::getBridgeSuccessPage() {
  return R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <title>Bridge gespeichert</title>
  <style>
    body { font-family: Arial, sans-serif; background:#eef2ff; height:100vh; margin:0; display:flex; align-items:center; justify-content:center; }
    .box { background:#fff; padding:30px; border-radius:12px; box-shadow:0 15px 35px rgba(0,0,0,.2); text-align:center; }
    h1 { margin:0 0 10px; color:#1f2937; }
    p { margin:0; color:#4b5563; }
  </style>
  <script>setTimeout(function(){window.location.href='/'},1500);</script>
</head>
<body>
  <div class="box">
    <h1>Bridge-Konfiguration gespeichert</h1>
    <p>Die Daten wurden per MQTT uebertragen.</p>
  </div>
</body>
</html>)html";
}

String WebAdminServer::getStatusJSON() {
  const DeviceConfig& cfg = configManager.getConfig();
  String json = "{";
  json += "\"wifi_connected\":";
  json += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  json += ",\"wifi_ssid\":\"" + String(cfg.wifi_ssid) + "\"";
  json += ",\"wifi_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"mqtt_host\":\"" + String(cfg.mqtt_host) + "\"";
  json += ",\"mqtt_port\":" + String(cfg.mqtt_port);
  json += ",\"mqtt_base\":\"" + String(cfg.mqtt_base_topic) + "\"";
  json += ",\"ha_prefix\":\"" + String(cfg.ha_prefix) + "\"";
  json += ",\"bridge_configured\":" + String(haBridgeConfig.hasData() ? "true" : "false");
  json += ",\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";
  return json;
}


