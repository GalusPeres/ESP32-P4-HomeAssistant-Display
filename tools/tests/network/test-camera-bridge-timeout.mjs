import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const read = (relativePath) => fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');

const popup = read('src/ui/popups/camera/camera_popup.cpp');
const stringsHeader = read('src/core/i18n/i18n.h');
const stringsSource = read('src/core/i18n/i18n.cpp');
const mqtt = read('src/network/mqtt/mqtt_handlers.cpp');

assert.match(popup, /kBridgeResponseTimeoutMs\s*=\s*30000/);
assert.ok(
  popup.includes('camera_popup_set_status(camera_text().camera_bridge_no_response, true);'),
  'silence must be reported as a liveness failure',
);
assert.ok(
  popup.includes('camera_text().camera_bridge_update_required'),
  'an explicit incompatible protocol must still show the upgrade message',
);
assert.match(
  popup,
  /protocol_field\.isNull\(\)[\s\S]*?camera_invalid_response[\s\S]*?protocol_field\.as<uint8_t>\(\)/,
  'a missing or malformed protocol must be invalid, not an inferred upgrade requirement',
);
assert.match(
  popup,
  /void camera_popup_set_status\([\s\S]*?if \(error && g_camera_popup\)[\s\S]*?waiting_for_bridge = false;[\s\S]*?bridge_response_deadline_ms = 0;/,
  'specific MQTT and parse errors must cancel the generic response deadline',
);
assert.ok(stringsHeader.includes('camera_bridge_no_response'));
for (const translation of [
  'Keine Kamera-Antwort',
  'No camera response',
  'Aucune réponse caméra',
]) {
  assert.ok(stringsSource.includes(translation), `missing translation: ${translation}`);
}
for (const specificError of [
  'camera_mqtt_disconnected',
  'camera_mqtt_topic_missing',
  'camera_mqtt_queue_full',
]) {
  assert.ok(mqtt.includes(specificError), `missing MQTT error path: ${specificError}`);
}

console.log('Camera bridge timeout contract: PASS');
