import fs from 'node:fs';

const read = path => fs.readFileSync(new URL(path, import.meta.url), 'utf8');
const switchStateHeader = read('../../../src/types/switch/state.h');
const tileRenderer = read('../../../src/tiles/runtime/tile_renderer.cpp');
const lightHeader = read('../../../src/ui/popups/light/light_popup.h');
const lightPopup = read('../../../src/ui/popups/light/light_popup.cpp');
const switchRenderer = read('../../../src/types/switch/renderer.cpp');

if (!/struct SwitchState \{\s*bool available = true;/s.test(switchStateHeader) ||
    !lightHeader.includes('bool available = true;')) {
  throw new Error('Light availability is missing from a runtime state hop');
}
for (const marker of [
  'if (normalized_state == "unavailable")',
  'out.available = false;',
  'init.available = !init.is_light || state.available;',
  'const bool light_unavailable =',
  'lv_obj_add_state(widgets.switch_obj, LV_STATE_DISABLED);'
]) {
  if (!tileRenderer.includes(marker) && !switchRenderer.includes(marker)) {
    throw new Error(`Light tile availability contract is missing: ${marker}`);
  }
}

const directToggle = switchRenderer.slice(
  switchRenderer.indexOf('static void toggle_switch_tile('),
  switchRenderer.indexOf('static LightPopupInit build_light_popup_init(')
);
if (!directToggle.includes('is_light_entity_id(data->entity_id)') ||
    !directToggle.includes('if (!current.available) return;')) {
  throw new Error('Unavailable Light still reaches the direct tile toggle path');
}
if (!switchRenderer.includes(
      'init.available = !init.is_light || state.available;')) {
  throw new Error('Light popup builder dropped availability');
}

for (const marker of [
  'ctx->available = !init.is_light || init.available;',
  'if (!ctx->available) {',
  'cancel_pending_live_publish(ctx);',
  'next_available != g_light_popup_ctx->available',
  'set_control_disabled(ctx->power_button, !ctx->available);',
  'if (!ctx || ctx->suppress_events || !ctx->available)',
  'get_unavailable_text()',
  '!ctx->available || !ctx->is_on'
]) {
  if (!lightPopup.includes(marker)) {
    throw new Error(`Light popup availability guard is missing: ${marker}`);
  }
}

const temperatureRelease = lightPopup.slice(
  lightPopup.indexOf('static void on_temp_track_event('),
  lightPopup.indexOf('static void apply_color_field_point(')
);
if (!temperatureRelease.includes('LV_EVENT_RELEASED') ||
    !temperatureRelease.includes('commit_color_temperature(ctx);')) {
  throw new Error('Light CCT slider no longer guarantees its final release value');
}
const temperatureCommit = lightPopup.slice(
  lightPopup.lastIndexOf('static void commit_color_temperature('),
  lightPopup.lastIndexOf('static void maybe_live_publish_color_temperature(')
);
if (!temperatureCommit.includes('cancel_pending_live_publish(ctx);') ||
    !temperatureCommit.includes('publish_color_temperature(ctx);')) {
  throw new Error('Light CCT release does not replace the throttled pending value');
}
for (const marker of [
  'ctx->min_color_temp_kelvin = init.min_color_temp_kelvin;',
  'ctx->max_color_temp_kelvin = init.max_color_temp_kelvin;',
  'clamp_color_temp_kelvin_to_range('
]) {
  if (!lightPopup.includes(marker)) {
    throw new Error(`Light CCT exact HA range handling is missing: ${marker}`);
  }
}

console.log('Light Home Assistant contract tests passed.');
