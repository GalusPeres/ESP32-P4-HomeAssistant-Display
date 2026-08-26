import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (relativePath) =>
  fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');

const renderer = read('src/tiles/tile_renderer.cpp');
const popup = read('src/ui/light_popup.cpp');
const mqtt = read('src/network/mqtt_handlers.cpp');

const updateStart = renderer.indexOf('void update_switch_tile_state(');
const queueStart = renderer.indexOf('void queue_switch_tile_update(', updateStart);
assert.ok(updateStart >= 0 && queueStart > updateStart,
          'Switch update function boundaries were not found');
const updateBody = renderer.slice(updateStart, queueStart);

const visualCheck = updateBody.indexOf('switch_tile_visual_state_equal(prev, state)');
const cacheWrite = updateBody.indexOf('state_target[grid_index] = state;');
const popupUpdate = updateBody.indexOf('update_light_popup(init);');
const visualReturn = updateBody.indexOf('if (tile_visual_unchanged) return;');
const firstStyleWrite = updateBody.indexOf('lv_obj_set_style_text_color');
assert.ok(visualCheck >= 0 && cacheWrite > visualCheck &&
          popupUpdate > cacheWrite && visualReturn > popupUpdate &&
          firstStyleWrite > visualReturn,
          'State cache and bound popup must update before duplicate tile redraw is skipped');

const popupStart = popup.indexOf('void update_light_popup(');
const preloadStart = popup.indexOf('void preload_light_popup()', popupStart);
assert.ok(popupStart >= 0 && preloadStart > popupStart,
          'Light popup update function boundaries were not found');
const popupBody = popup.slice(popupStart, preloadStart);
assert.match(popupBody,
             /has_tile_ref[\s\S]*tile_grid != init\.tile_grid[\s\S]*tile_index != init\.tile_index/,
             'Duplicate entities must not rebind an open popup to another tile');

const routeStart = mqtt.indexOf('static void rebuildDynamicRoutes(');
const weatherStart = mqtt.indexOf('static void rebuildDynamicWeatherRoutes(', routeStart);
assert.ok(routeStart >= 0 && weatherStart > routeStart,
          'Dynamic route function boundaries were not found');
const routeBody = mqtt.slice(routeStart, weatherStart);
assert.match(routeBody, /find_if[\s\S]*r\.topic == topic/,
             'Duplicate entities must retain one MQTT topic route');

const visibleState = {
  available: true,
  hasState: true,
  isOn: true,
  hasColor: false,
  color: 0,
  hasColorTemp: false,
  colorTemp: 4000,
  supportsColor: false,
  supportsBrightness: true,
  supportsTemperature: false,
  modesKnown: true,
  onOffOnly: false,
};
const visualSignature = (state) => JSON.stringify(visibleStateKeys(state));
const visibleStateKeys = (state) => ({
  available: state.available,
  hasState: state.hasState,
  isOn: state.isOn,
  hasColor: state.hasColor,
  color: state.hasColor ? state.color : null,
  hasColorTemp: state.hasColorTemp,
  colorTemp: state.hasColorTemp ? state.colorTemp : null,
  supportsColor: state.supportsColor,
  supportsBrightness: state.supportsBrightness,
  supportsTemperature: state.supportsTemperature,
  modesKnown: state.modesKnown,
  onOffOnly: state.onOffOnly,
});

const brightnessEcho = {...visibleState, brightness: 18};
const nextBrightnessEcho = {...visibleState, brightness: 77};
assert.equal(visualSignature(brightnessEcho), visualSignature(nextBrightnessEcho),
             'Brightness-only feedback must not redraw duplicate tiles');
assert.notEqual(visualSignature(visibleState),
                visualSignature({...visibleState, isOn: false}),
                'On/off feedback must still redraw every duplicate tile');
assert.notEqual(visualSignature(visibleState),
                visualSignature({...visibleState, hasColor: true, color: 0x336699}),
                'Color feedback must still redraw every duplicate tile');

console.log('Duplicate light entity update contract: PASS');
