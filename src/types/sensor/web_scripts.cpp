#include "src/types/sensor/web_scripts.h"

void append_sensor_scripts(String& html) {
  html += R"html(
  <script>
  function maybeFillTitleFromSensor(tab) {
    maybeFillTitleFromEntity(tab, '_sensor_entity');
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
    const valueFontSelect = document.getElementById(prefix + '_sensor_value_font');
    if (!entitySelect) return;
    const entity = entitySelect.value;
    if (!entity) {
      const valueElem = document.getElementById(tab + '-tile-' + currentTileIndex + '-value');
      if (valueElem) {
        const unit = unitInput ? unitInput.value : '';
        valueElem.innerHTML = '--' + (unit ? '<span class="tile-unit">' + unit + '</span>' : '');
        applySensorValueFontClass(valueElem, valueFontSelect ? valueFontSelect.value : '0');
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
          applySensorValueFontClass(valueElem, valueFontSelect ? valueFontSelect.value : '0');
        }
      })
      .catch(err => console.error('Fehler beim Laden des Sensorwerts:', err));
  }

  function normalizeSensorValueFont(value) {
    const v = String(value || '0');
    return (v === '1' || v === '2') ? v : '0';
  }

  function getSensorValueFontClass(value) {
    const v = normalizeSensorValueFont(value);
    if (v === '1') return 'sensor-value-size-20';
    if (v === '2') return 'sensor-value-size-24';
    return 'sensor-value-size-default';
  }

  function applySensorValueFontClass(el, value) {
    if (!el) return;
    el.classList.remove('sensor-value-size-20', 'sensor-value-size-24', 'sensor-value-size-default');
    el.classList.add(getSensorValueFontClass(value));
  }

  function syncGaugeUi(tab) {
    const prefix = tab;
    const typeValue = document.getElementById(prefix + '_tile_type')?.value || '0';
    const gaugeWrap = document.getElementById(prefix + '_sensor_gauge_fields');
    const graphWrap = document.getElementById(prefix + '_sensor_graph_fields');
    if (typeValue !== '1') {
      if (gaugeWrap) gaugeWrap.classList.add('hidden');
      if (graphWrap) graphWrap.classList.add('hidden');
      return;
    }
    const displayMode = document.getElementById(prefix + '_sensor_display_mode')?.value || '0';
    if (gaugeWrap) {
      if (displayMode === '1') gaugeWrap.classList.remove('hidden');
      else gaugeWrap.classList.add('hidden');
    }
    if (graphWrap) {
      if (displayMode === '2') graphWrap.classList.remove('hidden');
      else graphWrap.classList.add('hidden');
    }
  }

  function loadSensorFields(tab, data) {
    const prefix = tab;
    const entityEl = document.getElementById(prefix + '_sensor_entity');
    if (entityEl) entityEl.value = data.sensor_entity || '';
    const unitEl = document.getElementById(prefix + '_sensor_unit');
    if (unitEl) unitEl.value = data.sensor_unit || '';
    const decEl = document.getElementById(prefix + '_sensor_decimals');
    if (decEl) decEl.value = (data.sensor_decimals !== undefined && data.sensor_decimals >= 0) ? data.sensor_decimals : '';
    const fontEl = document.getElementById(prefix + '_sensor_value_font');
    if (fontEl) fontEl.value = (data.sensor_value_font !== undefined) ? String(data.sensor_value_font) : '0';
    const displayModeEl = document.getElementById(prefix + '_sensor_display_mode');
    if (displayModeEl) displayModeEl.value = (data.sensor_display_mode !== undefined) ? String(data.sensor_display_mode) : '0';
    const gaugeMinEl = document.getElementById(prefix + '_sensor_gauge_min');
    if (gaugeMinEl) gaugeMinEl.value = (data.sensor_gauge_min !== undefined && data.sensor_gauge_min !== null) ? String(data.sensor_gauge_min) : '';
    const gaugeMaxEl = document.getElementById(prefix + '_sensor_gauge_max');
    if (gaugeMaxEl) gaugeMaxEl.value = (data.sensor_gauge_max !== undefined && data.sensor_gauge_max !== null) ? String(data.sensor_gauge_max) : '';
    const gaugeArcEl = document.getElementById(prefix + '_sensor_gauge_arc');
    if (gaugeArcEl) gaugeArcEl.value = (data.sensor_gauge_arc !== undefined && data.sensor_gauge_arc !== null) ? String(data.sensor_gauge_arc) : '';
    const gaugeSizeEl = document.getElementById(prefix + '_sensor_gauge_size');
    if (gaugeSizeEl) gaugeSizeEl.value = (data.sensor_gauge_size !== undefined && data.sensor_gauge_size !== null) ? String(data.sensor_gauge_size) : '';
    const gaugeYOffsetEl = document.getElementById(prefix + '_sensor_gauge_y_offset');
    if (gaugeYOffsetEl) gaugeYOffsetEl.value = (data.sensor_gauge_y_offset !== undefined && data.sensor_gauge_y_offset !== null) ? String(data.sensor_gauge_y_offset) : '';
    const valueYOffsetEl = document.getElementById(prefix + '_sensor_value_y_offset');
    if (valueYOffsetEl) valueYOffsetEl.value = (data.sensor_value_y_offset !== undefined && data.sensor_value_y_offset !== null) ? String(data.sensor_value_y_offset) : '';
    const graphHeightEl = document.getElementById(prefix + '_sensor_graph_height');
    if (graphHeightEl) graphHeightEl.value = (data.sensor_graph_height !== undefined && data.sensor_graph_height !== null) ? String(data.sensor_graph_height) : '';
    syncGaugeUi(tab);
  }

  function saveSensorFields(tab, formData) {
    const prefix = tab;
    formData.append('sensor_entity', document.getElementById(prefix + '_sensor_entity')?.value || '');
    formData.append('sensor_unit', document.getElementById(prefix + '_sensor_unit')?.value || '');
    formData.append('sensor_decimals', document.getElementById(prefix + '_sensor_decimals')?.value || '');
    formData.append('sensor_value_font', document.getElementById(prefix + '_sensor_value_font')?.value || '0');
    formData.append('sensor_display_mode', document.getElementById(prefix + '_sensor_display_mode')?.value || '0');
    formData.append('sensor_gauge_min', document.getElementById(prefix + '_sensor_gauge_min')?.value || '');
    formData.append('sensor_gauge_max', document.getElementById(prefix + '_sensor_gauge_max')?.value || '');
    formData.append('sensor_gauge_arc', document.getElementById(prefix + '_sensor_gauge_arc')?.value || '');
    formData.append('sensor_gauge_size', document.getElementById(prefix + '_sensor_gauge_size')?.value || '');
    formData.append('sensor_gauge_y_offset', document.getElementById(prefix + '_sensor_gauge_y_offset')?.value || '');
    formData.append('sensor_value_y_offset', document.getElementById(prefix + '_sensor_value_y_offset')?.value || '');
    formData.append('sensor_graph_height', document.getElementById(prefix + '_sensor_graph_height')?.value || '');
  }

  function resetSensorFields(tab) {
    const prefix = tab;
    const entityEl = document.getElementById(prefix + '_sensor_entity');
    if (entityEl) entityEl.value = '';
    const unitEl = document.getElementById(prefix + '_sensor_unit');
    if (unitEl) unitEl.value = '';
    const decEl = document.getElementById(prefix + '_sensor_decimals');
    if (decEl) decEl.value = '';
    const fontEl = document.getElementById(prefix + '_sensor_value_font');
    if (fontEl) fontEl.value = '0';
    const displayModeEl = document.getElementById(prefix + '_sensor_display_mode');
    if (displayModeEl) displayModeEl.value = '0';
    const gaugeMinEl = document.getElementById(prefix + '_sensor_gauge_min');
    if (gaugeMinEl) gaugeMinEl.value = '';
    const gaugeMaxEl = document.getElementById(prefix + '_sensor_gauge_max');
    if (gaugeMaxEl) gaugeMaxEl.value = '';
    const gaugeArcEl = document.getElementById(prefix + '_sensor_gauge_arc');
    if (gaugeArcEl) gaugeArcEl.value = '';
    const gaugeSizeEl = document.getElementById(prefix + '_sensor_gauge_size');
    if (gaugeSizeEl) gaugeSizeEl.value = '';
    const gaugeYOffsetEl = document.getElementById(prefix + '_sensor_gauge_y_offset');
    if (gaugeYOffsetEl) gaugeYOffsetEl.value = '';
    const valueYOffsetEl = document.getElementById(prefix + '_sensor_value_y_offset');
    if (valueYOffsetEl) valueYOffsetEl.value = '';
    const graphHeightEl = document.getElementById(prefix + '_sensor_graph_height');
    if (graphHeightEl) graphHeightEl.value = '';
  }
  </script>
)html";
}
