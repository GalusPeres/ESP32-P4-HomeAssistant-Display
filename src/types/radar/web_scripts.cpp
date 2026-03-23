#include "src/types/radar/web_scripts.h"

void append_radar_scripts(String& html) {
  html += R"html(
  <script>
  function getRadarPresetLabel(value) {
    switch (String(value || 'bayern')) {
      case 'germany': return 'Deutschland';
      case 'south': return 'Sueddeutschland';
      case 'custom': return 'Radar';
      case 'bayern':
      default: return 'Bayern';
    }
  }

  function maybeFillTitleFromRadar(tab) {
    const titleEl = document.getElementById(tab + '_tile_title');
    if (!titleEl) return;
    const current = String(titleEl.value || '').trim();
    if (current.length) return;
    const preset = document.getElementById(tab + '_radar_preset')?.value || 'bayern';
    titleEl.value = 'Radar ' + getRadarPresetLabel(preset);
  }

  function syncRadarUi(tab) {
    const prefix = tab;
    const presetEl = document.getElementById(prefix + '_radar_preset');
    const bboxEl = document.getElementById(prefix + '_radar_bbox');
    if (!bboxEl) return;
    const custom = (presetEl?.value || 'bayern') === 'custom';
    bboxEl.disabled = !custom;
    bboxEl.parentElement?.classList.toggle('field-disabled', !custom);
  }

  function loadRadarFields(tab, data) {
    const prefix = tab;
    const presetEl = document.getElementById(prefix + '_radar_preset');
    const bboxEl = document.getElementById(prefix + '_radar_bbox');
    const frameCountEl = document.getElementById(prefix + '_radar_frame_count');
    const frameDelayEl = document.getElementById(prefix + '_radar_frame_delay_ms');
    const refreshEl = document.getElementById(prefix + '_radar_refresh_sec');
    const popupModeEl = document.getElementById(prefix + '_radar_popup_open_mode');
    if (presetEl) presetEl.value = data.sensor_entity || data.radar_preset || 'bayern';
    if (bboxEl) bboxEl.value = data.sensor_unit || data.radar_bbox || '';
    if (frameCountEl) frameCountEl.value = data.sensor_gauge_min || data.radar_frame_count || '6';
    if (frameDelayEl) frameDelayEl.value = data.sensor_gauge_max || data.radar_frame_delay_ms || '450';
    if (refreshEl) refreshEl.value = data.image_slideshow_sec || data.radar_refresh_sec || '300';
    if (popupModeEl) popupModeEl.value = (data.popup_open_mode !== undefined) ? String(data.popup_open_mode) : '0';
    syncRadarUi(tab);
    maybeFillTitleFromRadar(tab);
  }

  function saveRadarFields(tab, formData) {
    const prefix = tab;
    formData.append('radar_preset', document.getElementById(prefix + '_radar_preset')?.value || 'bayern');
    formData.append('radar_bbox', document.getElementById(prefix + '_radar_bbox')?.value || '');
    formData.append('radar_frame_count', document.getElementById(prefix + '_radar_frame_count')?.value || '6');
    formData.append('radar_frame_delay_ms', document.getElementById(prefix + '_radar_frame_delay_ms')?.value || '450');
    formData.append('radar_refresh_sec', document.getElementById(prefix + '_radar_refresh_sec')?.value || '300');
    formData.append('popup_open_mode', document.getElementById(prefix + '_radar_popup_open_mode')?.value || '0');
  }

  function resetRadarFields(tab) {
    const prefix = tab;
    const presetEl = document.getElementById(prefix + '_radar_preset');
    const bboxEl = document.getElementById(prefix + '_radar_bbox');
    const frameCountEl = document.getElementById(prefix + '_radar_frame_count');
    const frameDelayEl = document.getElementById(prefix + '_radar_frame_delay_ms');
    const refreshEl = document.getElementById(prefix + '_radar_refresh_sec');
    const popupModeEl = document.getElementById(prefix + '_radar_popup_open_mode');
    if (presetEl) presetEl.value = 'bayern';
    if (bboxEl) bboxEl.value = '';
    if (frameCountEl) frameCountEl.value = '6';
    if (frameDelayEl) frameDelayEl.value = '450';
    if (refreshEl) refreshEl.value = '300';
    if (popupModeEl) popupModeEl.value = '0';
    syncRadarUi(tab);
  }
  </script>
)html";
}
