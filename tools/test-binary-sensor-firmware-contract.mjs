import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = relativePath =>
  fs.readFileSync(path.join(repoRoot, relativePath), 'utf8')
    .replace(/\r\n?/g, '\n');

const tileConfigHeader = read('src/tiles/tile_config.h');
const tileConfig = read('src/tiles/tile_config.cpp');
const tileRendererHeader = read('src/tiles/tile_renderer.h');
const tileRenderer = read('src/tiles/tile_renderer.cpp');
const binaryRenderer = read('src/types/binary_sensor/renderer.cpp');
const registry = read('src/types/types_registry.cpp');
const bridge = read('src/network/ha_bridge_config.cpp');
const bridgeHeader = read('src/network/ha_bridge_config.h');
const mqtt = read('src/network/mqtt_handlers.cpp');
const tiles = read('src/ui/tab_tiles_unified.cpp');
const screensaver = read('src/ui/image_screensaver.cpp');
const screensaverConfig = read('src/ui/screensaver_config.cpp');
const sketch = read('HomeTiles.ino');

assert.match(tileConfigHeader, /TILE_COVER = 19,\s*TILE_BINARY_SENSOR = 20/,
             'Binary Sensor must append stable TileType ID 20');

const packedV7 = tileConfig.match(/struct PackedTileV7 \{[\s\S]*?\n\};/)?.[0] ?? '';
assert.ok(packedV7.includes('char sensor_entity[ENTITY_MAX];'),
          'PackedTileV7 must continue to reuse sensor_entity');
assert.doesNotMatch(packedV7, /binary_sensor|device_class|last_changed/,
                    'PackedTileV7 layout must not grow for Binary Sensor');
for (const marker of [
  'type == TILE_BINARY_SENSOR',
  'in.type == TILE_BINARY_SENSOR',
  'out.type == TILE_BINARY_SENSOR'
]) {
  assert.ok(tileConfig.includes(marker),
            `Binary Sensor persistence path is missing: ${marker}`);
}
assert.match(tileConfigHeader,
             /tile\.type != TILE_COVER && tile\.type != TILE_BINARY_SENSOR/,
             'Binary Sensor popup mode must survive Tile round trips');

for (const marker of [
  'bool has_available = false;',
  'bool has_device_class = false;',
  'bool has_last_changed = false;',
  'bool has_icon = false;',
  'uint64_t last_changed = 0;',
  'char device_class[24] = {};',
  'char icon_name[40] = {};',
  'BINARY_SENSOR_PAYLOAD_MAX = 512'
]) {
  assert.ok(tileRendererHeader.includes(marker),
            `Bounded Binary Sensor state field is missing: ${marker}`);
}

for (const state of ['off', 'on', 'unknown', 'unavailable']) {
  assert.ok(binaryRenderer.includes(`state == "${state}"`) ||
            binaryRenderer.includes(`return "${state}"`),
            `Canonical state is missing from the firmware parser: ${state}`);
}
for (const marker of [
  'available.is<bool>()',
  'out.has_available = true;',
  'out.has_device_class = true;',
  'out.has_last_changed = true;',
  'out.has_icon = true;',
  'value == BinarySensorValue::Unavailable) {',
  'i18n::binary_sensor_state_label(',
  'queue_binary_sensor_popup_state('
]) {
  assert.ok((binaryRenderer + tiles).includes(marker),
            `Binary Sensor semantic state path is incomplete: ${marker}`);
}
assert.match(binaryRenderer,
             /!state\.valid \|\| state\.value == BinarySensorValue::Missing/,
             'An explicitly missing Binary Sensor must render as no value');

const officialIcons = new Map([
  ['', ['radiobox-blank', 'checkbox-marked-circle']],
  ['battery', ['battery', 'battery-outline']],
  ['battery_charging', ['battery', 'battery-charging']],
  ['carbon_monoxide', ['smoke-detector', 'smoke-detector-alert']],
  ['cold', ['thermometer', 'snowflake']],
  ['connectivity', ['close-network-outline', 'check-network-outline']],
  ['door', ['door-closed', 'door-open']],
  ['garage_door', ['garage', 'garage-open']],
  ['gas', ['check-circle', 'alert-circle']],
  ['heat', ['thermometer', 'fire']],
  ['light', ['brightness-5', 'brightness-7']],
  ['lock', ['lock', 'lock-open']],
  ['moisture', ['water-off', 'water']],
  ['motion', ['motion-sensor-off', 'motion-sensor']],
  ['moving', ['octagon', 'arrow-right']],
  ['occupancy', ['home-outline', 'home']],
  ['opening', ['square', 'square-outline']],
  ['plug', ['power-plug-off', 'power-plug']],
  ['power', ['power-plug-off', 'power-plug']],
  ['presence', ['home-outline', 'home']],
  ['problem', ['check-circle', 'alert-circle']],
  ['running', ['stop', 'play']],
  ['safety', ['check-circle', 'alert-circle']],
  ['smoke', ['smoke-detector-variant', 'smoke-detector-variant-alert']],
  ['sound', ['music-note-off', 'music-note']],
  ['tamper', ['check-circle', 'alert-circle']],
  ['update', ['package', 'package-up']],
  ['vibration', ['crop-portrait', 'vibrate']],
  ['window', ['window-closed', 'window-open']],
]);
for (const [deviceClass, [offIcon, onIcon]] of officialIcons) {
  const row = `{"${deviceClass}", "${offIcon}", "${onIcon}"}`;
  assert.ok(binaryRenderer.includes(row),
            `Home Assistant icon pair is missing: ${row}`);
}
assert.match(binaryRenderer,
             /if \(configured\.length\(\)\) return configured;[\s\S]*if \(dynamic_icon\) \*dynamic_icon = true;[\s\S]*if \(state\.has_icon\)/,
             'An explicit tile icon must win before live HA icon selection');
assert.match(binaryRenderer,
             /if \(state\.has_icon\)[\s\S]*if \(normalized\.length\(\)\) return normalized;/,
             'The live HA-resolved Binary Sensor icon must be preserved');
assert.match(binaryRenderer,
             /state\.valid && state\.available &&\s*state\.value == BinarySensorValue::On/,
             'Only an available on state may select the active icon');

for (const marker of [
  'TILE_BINARY_SENSOR,',
  'render_binary_sensor_wrapper',
  'apply_binary_sensor_fields_from_request',
  'append_binary_sensor_fields_html(',
  'append_binary_sensor_styles',
  'append_binary_sensor_scripts',
  'i18n::binary_sensor_label(language, 0)'
]) {
  assert.ok(registry.includes(marker),
            `Binary Sensor registry integration is missing: ${marker}`);
}

for (const marker of [
  'String binary_sensors_text;',
  'binary_sensors',
  'binary_sensor_meta',
  'parseBinarySensorMetaSection(',
  'state_doc["available"]',
  'state_doc["device_class"]',
  'state_doc["icon"]',
  'state_doc["last_changed"]',
  'parseEntityIconSection(body, "binary_sensor_meta", icons)'
]) {
  assert.ok((bridgeHeader + bridge).includes(marker),
            `Bridge Binary Sensor metadata path is missing: ${marker}`);
}
assert.match(bridge,
             /state_doc\["state"\] = nullptr;[\s\S]*upsertKeyValueMap\(values, entity, state_payload\)/,
             'An absent HA entity must replace stale state with explicit JSON null');
assert.match(binaryRenderer,
             /root\.containsKey\("state"\)[\s\S]*state_variant\.isNull\(\)[\s\S]*out\.value = value/,
             'The firmware parser must preserve explicit missing separately');
assert.match(binaryRenderer,
             /strnlen\(payload, BINARY_SENSOR_PAYLOAD_MAX \+ 1\)[\s\S]*BINARY_SENSOR_PAYLOAD_MAX/,
             'Binary Sensor queue payloads must be bounded before String copies');
assert.match(mqtt,
             /payload_len > BINARY_SENSOR_PAYLOAD_MAX[\s\S]*Oversized state dropped[\s\S]*return true;/,
             'Oversized Binary Sensor MQTT payloads must be dropped before caching');
assert.match(binaryRenderer,
             /value_double < 18446744073709551616\.0/,
             'Floating timestamps must be bounded before conversion to uint64_t');

for (const source of [mqtt, tiles, screensaverConfig]) {
  assert.ok(source.includes('TILE_BINARY_SENSOR'),
            'A dynamic route, runtime cache, or screensaver allow-list is incomplete');
}
for (const marker of [
  'queue_binary_sensor_tile_update(',
  'queue_binary_sensor_tile_updates(',
  'process_binary_sensor_update_queue(',
  'reset_binary_sensor_widget(',
  'reset_binary_sensor_widgets('
]) {
  assert.ok((tiles + screensaver + sketch).includes(marker),
            `Binary Sensor runtime lifecycle hook is missing: ${marker}`);
}
for (const marker of [
  'memcpy(out->binary_sensors,',
  'memcpy(out->binary_sensor_states,',
  'in->binary_sensors',
  'in->binary_sensor_states'
]) {
  assert.ok(tileRenderer.includes(marker),
            `Folder widget snapshot path is missing: ${marker}`);
}

const processStart = binaryRenderer.indexOf(
  'void process_binary_sensor_update_queue(');
assert.ok(processStart >= 0, 'Binary Sensor bounded queue processor is missing');
const processBody = binaryRenderer.slice(processStart);
const parseAt = processBody.indexOf('parse_binary_sensor_payload(');
const fanoutAt = processBody.indexOf('for (uint8_t index = 0;');
const applyAt = processBody.indexOf('apply_state(', fanoutAt);
assert.ok(parseAt >= 0 && fanoutAt > parseAt && applyAt > fanoutAt,
          'One parsed Binary Sensor payload must fan out to duplicate tiles');
assert.match(processBody,
             /processed >= max_updates[\s\S]*update\.grid_indices != 0[\s\S]*return;/,
             'Binary Sensor processing must preserve a partially applied batch');
assert.match(binaryRenderer,
             /update\.layout_generation != layout_generation\(update\.grid_type\)[\s\S]*update\.grid_indices = 0;/,
             'Stale Binary Sensor work must be dropped after a layout change');

const hiddenCacheBuildAt = tiles.indexOf(
  'static void build_folder_cache_entry(');
const hiddenCacheBinaryDrainAt = tiles.indexOf(
  'process_binary_sensor_update_queue();', hiddenCacheBuildAt);
const hiddenCacheClearAt = tiles.indexOf(
  'clear_cache_entry(entry);', hiddenCacheBuildAt);
const hiddenCacheRenderAt = tiles.indexOf(
  'render_tile_grid(entry.grid', hiddenCacheBuildAt);
assert.ok(hiddenCacheBuildAt >= 0 &&
          hiddenCacheBinaryDrainAt > hiddenCacheBuildAt &&
          hiddenCacheBinaryDrainAt < hiddenCacheClearAt &&
          hiddenCacheClearAt < hiddenCacheRenderAt,
          'Pending Binary Sensor updates must drain before hidden-folder widget swaps');

const releaseAllAt = tiles.indexOf('void tiles_release_all()');
const releaseAllEnd = tiles.indexOf('bool tiles_is_loaded(', releaseAllAt);
const releaseAllBody = tiles.slice(releaseAllAt, releaseAllEnd);
const releaseAllBinaryResetAt = releaseAllBody.indexOf(
  'reset_binary_sensor_widgets(GridType::TAB0);');
const releaseAllCacheDeleteAt = releaseAllBody.indexOf(
  'for (size_t i = 0; i < g_folder_cache_slot_count; ++i)');
const releaseAllTab0NullAt = releaseAllBody.indexOf(
  'g_tiles_grids[0] = nullptr;');
assert.ok(releaseAllAt >= 0 && releaseAllEnd > releaseAllAt &&
          releaseAllBinaryResetAt >= 0 &&
          releaseAllBinaryResetAt < releaseAllCacheDeleteAt &&
          releaseAllBinaryResetAt < releaseAllTab0NullAt,
          'Release-all must invalidate TAB0 Binary Sensor bindings before deleting their grids');

const parseState = payload => {
  if (typeof payload === 'string') payload = {state: payload};
  const state = String(payload.state ?? '').trim().toLowerCase();
  assert.ok(['on', 'off', 'unknown', 'unavailable'].includes(state));
  const hasAvailable = typeof payload.available === 'boolean';
  const available = state === 'unavailable'
    ? false
    : (hasAvailable ? payload.available : true);
  const hasDeviceClass = typeof payload.device_class === 'string';
  const hasLastChanged = Number.isSafeInteger(payload.last_changed) &&
    payload.last_changed >= 0;
  return {
    state,
    available,
    hasAvailable,
    deviceClass: hasDeviceClass ? payload.device_class : '',
    hasDeviceClass,
    lastChanged: hasLastChanged ? payload.last_changed : 0,
    hasLastChanged,
  };
};

assert.deepEqual(parseState('unknown'), {
  state: 'unknown', available: true, hasAvailable: false,
  deviceClass: '', hasDeviceClass: false,
  lastChanged: 0, hasLastChanged: false,
});
assert.deepEqual(parseState({
  state: 'on', available: false, device_class: 'motion',
  last_changed: 1788431570,
}), {
  state: 'on', available: false, hasAvailable: true,
  deviceClass: 'motion', hasDeviceClass: true,
  lastChanged: 1788431570, hasLastChanged: true,
});
assert.equal(parseState({state: 'unavailable', available: true}).available,
             false, 'Unavailable must stay unavailable despite bad metadata');
assert.equal(parseState({state: 'off', last_changed: null}).hasLastChanged,
             false, 'A null timestamp must not become Unix epoch zero');

const iconFor = (state, deviceClass = '') => {
  const [offIcon, onIcon] = officialIcons.get(deviceClass) ?? officialIcons.get('');
  return state === 'on' ? onIcon : offIcon;
};
assert.equal(iconFor('on', 'motion'), 'motion-sensor');
assert.equal(iconFor('off', 'motion'), 'motion-sensor-off');
assert.equal(iconFor('unknown', 'door'), 'door-closed');
assert.equal(iconFor('unavailable', 'window'), 'window-closed');

console.log('Binary Sensor firmware contract: PASS');
