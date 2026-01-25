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
  </script>
)html";
}
