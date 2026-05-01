#include "src/types/media/web_scripts.h"

void append_media_scripts(String& html) {
  html += R"html(
  <script>
  function maybeFillTitleFromMedia(tab) {
    maybeFillTitleFromEntity(tab, '_media_entity');
  }

  function parseMediaPreviewPayload(value) {
    const out = { title: '--', subtitle: '--', state: '--' };
    if (value === undefined || value === null) return out;
    const text = String(value).trim();
    if (!text.length) return out;
    if (text.startsWith('{')) {
      try {
        const obj = JSON.parse(text);
        if (obj && typeof obj === 'object') {
          out.title = obj.media_title || obj.media_channel || obj.state || '--';
          out.subtitle = obj.media_artist || obj.media_album_name || obj.app_name || obj.source || '--';
          out.state = obj.state || '--';
          if (obj.volume_level !== undefined && obj.volume_level !== null) {
            const pct = Math.max(0, Math.min(100, Math.round(Number(obj.volume_level) * 100)));
            out.state = (out.state && out.state !== '--') ? (out.state + '  ' + pct + '%') : (pct + '%');
          }
        }
      } catch (e) {}
      return out;
    }
    out.title = text;
    out.state = text;
    return out;
  }

  function updateMediaValuePreview(tab) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const entitySelect = document.getElementById(prefix + '_media_entity');
    if (!entitySelect) return;
    const entity = entitySelect.value;
    const tileElem = document.getElementById(tab + '-tile-' + currentTileIndex);
    if (!tileElem) return;
    const applyMeta = (meta) => {
      const values = (meta && meta.values) || {};
      const parsed = parseMediaPreviewPayload(entity ? (values[entity] ?? '') : '');
      const titleEl = tileElem.querySelector('.tile-media-title');
      const subEl = tileElem.querySelector('.tile-media-subtitle');
      const stateEl = tileElem.querySelector('.tile-media-state');
      if (titleEl) titleEl.textContent = parsed.title || '--';
      if (subEl) subEl.textContent = parsed.subtitle || '--';
      if (stateEl) stateEl.textContent = parsed.state || '--';
    };
    const metaPromise = isSensorMetaCacheLoaded() ? Promise.resolve(sensorMetaCache) : fetchSensorMetaCache();
    metaPromise
      .then(meta => applyMeta(meta))
      .catch(err => console.error('Fehler beim Laden des Media-Status:', err));
  }

  function loadMediaFields(tab, data) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_media_entity');
    if (el) el.value = data.sensor_entity || data.media_entity || '';
    maybeFillTitleFromMedia(tab);
    updateMediaValuePreview(tab);
  }

  function saveMediaFields(tab, formData) {
    const prefix = tab;
    const entity = document.getElementById(prefix + '_media_entity')?.value || '';
    formData.append('media_entity', entity);
    formData.append('sensor_entity', entity);
  }

  function resetMediaFields(tab) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_media_entity');
    if (el) el.value = '';
  }
  </script>
)html";
}
