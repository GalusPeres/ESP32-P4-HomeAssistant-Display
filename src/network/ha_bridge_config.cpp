#include "src/network/ha_bridge_config.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <stdio.h>

static const char* PREF_NAMESPACE = "tab5_config";
static void logList(const char* label, const String& text);
static bool sensorExistsInList(const String& list, const String& candidate);
static bool aliasExistsInList(const String& list, const String& alias);
static String normalizeLineLocal(const String& line);
static int countListEntries(const String& text);
static int countMapEntries(const String& text);
static bool listEqualsIgnoringOrder(const String& a, const String& b);
static bool mapEqualsIgnoringOrder(const String& a, const String& b);
static bool bridgeConfigEquals(const HaBridgeConfigData& a, const HaBridgeConfigData& b);
static void parseSensorMetaSection(const String& body, String& units, String& names, String& values);
static void parseEntityNameSection(const String& body, const char* key, String& names);
static void parseIconMetaSections(const String& body, String& icons);
static void parseEnergySection(const String& body,
                               String& energy,
                               String& units,
                               String& names,
                               String& icons);
static int findMatchingJsonObjectEnd(const String& body, int object_start);
static bool extractStringField(const String& object, const char* key, String& out);
static String lookupKeyValue(const String& text, const String& key);
static void upsertKeyValueMap(String& text, const String& key, const String& value);
static bool removeKeyValueMapEntry(String& text, const String& key);
static String decodeJsonEscapes(const String& value);
static void appendUtf8(String& out, uint32_t codepoint);
static bool isHexDigit(char c);
static uint8_t hexValue(char c);

HaBridgeConfig haBridgeConfig;

HaBridgeConfig::HaBridgeConfig() = default;

bool HaBridgeConfig::load() {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    return false;
  }

  data.sensors_text = "";
  data.energy_text = "";
  data.weathers_text = "";
  data.lights_text = "";
  data.switches_text = "";
  data.media_players_text = "";
  data.scene_alias_text = "";
  data.sensor_units_map = "";
  data.sensor_names_map = "";
  data.sensor_values_map = "";
  data.entity_icons_map = "";
  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    char key[12];
    snprintf(key, sizeof(key), "slot_s%u", static_cast<unsigned>(i));
    data.sensor_slots[i] = prefs.getString(key, "");
    snprintf(key, sizeof(key), "title_s%u", static_cast<unsigned>(i));
    data.sensor_titles[i] = prefs.getString(key, "");
    snprintf(key, sizeof(key), "unit_s%u", static_cast<unsigned>(i));
    data.sensor_custom_units[i] = prefs.getString(key, "");
    snprintf(key, sizeof(key), "color_s%u", static_cast<unsigned>(i));
    data.sensor_colors[i] = prefs.getUInt(key, 0);
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    char key[12];
    snprintf(key, sizeof(key), "slot_c%u", static_cast<unsigned>(i));
    data.scene_slots[i] = prefs.getString(key, "");
    snprintf(key, sizeof(key), "title_c%u", static_cast<unsigned>(i));
    data.scene_titles[i] = prefs.getString(key, "");
    snprintf(key, sizeof(key), "color_c%u", static_cast<unsigned>(i));
    data.scene_colors[i] = prefs.getUInt(key, 0);
  }

  prefs.end();
  return true;
}

bool HaBridgeConfig::save(const HaBridgeConfigData& incoming) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    return false;
  }

  // WICHTIG: Listen NICHT in NVS speichern - die kommen per MQTT-Discovery!
  // Nur die Zuordnungen (Slots) speichern, sonst wird NVS bei vielen Sensoren zu groß!
  // prefs.putString("ha_sensors", incoming.sensors_text);
  // prefs.putString("ha_scene_alias", incoming.scene_alias_text);
  // prefs.putString("ha_sens_units", incoming.sensor_units_map);
  // prefs.putString("ha_sens_names", incoming.sensor_names_map);
  // prefs.putString("ha_sens_vals", incoming.sensor_values_map);
  // HA-Auto-Icons are runtime metadata. Manual tile icons are stored with the
  // tile config, but HA icon changes must not write to flash.
  prefs.remove("ha_sensors");
  prefs.remove("ha_scene_alias");
  prefs.remove("ha_sens_units");
  prefs.remove("ha_sens_names");
  prefs.remove("ha_sens_vals");
  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    char key[12];
    snprintf(key, sizeof(key), "slot_s%u", static_cast<unsigned>(i));
    prefs.putString(key, incoming.sensor_slots[i]);
    snprintf(key, sizeof(key), "title_s%u", static_cast<unsigned>(i));
    prefs.putString(key, incoming.sensor_titles[i]);
    snprintf(key, sizeof(key), "unit_s%u", static_cast<unsigned>(i));
    prefs.putString(key, incoming.sensor_custom_units[i]);
    snprintf(key, sizeof(key), "color_s%u", static_cast<unsigned>(i));
    prefs.putUInt(key, incoming.sensor_colors[i]);
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    char key[12];
    snprintf(key, sizeof(key), "slot_c%u", static_cast<unsigned>(i));
    prefs.putString(key, incoming.scene_slots[i]);
    snprintf(key, sizeof(key), "title_c%u", static_cast<unsigned>(i));
    prefs.putString(key, incoming.scene_titles[i]);
    snprintf(key, sizeof(key), "color_c%u", static_cast<unsigned>(i));
    prefs.putUInt(key, incoming.scene_colors[i]);
  }
  prefs.end();

  data = incoming;
  return true;
}

bool HaBridgeConfig::hasData() const {
  return data.sensors_text.length() > 0 ||
         data.energy_text.length() > 0 ||
         data.weathers_text.length() > 0 ||
         data.lights_text.length() > 0 ||
         data.switches_text.length() > 0 ||
         data.media_players_text.length() > 0 ||
         data.scene_alias_text.length() > 0;
}

String HaBridgeConfig::findSensorUnit(const String& entity_id) const {
  return lookupKeyValue(data.sensor_units_map, entity_id);
}

String HaBridgeConfig::findSensorName(const String& entity_id) const {
  return lookupKeyValue(data.sensor_names_map, entity_id);
}

String HaBridgeConfig::findSensorInitialValue(const String& entity_id) const {
  return lookupKeyValue(data.sensor_values_map, entity_id);
}

String HaBridgeConfig::findEntityIcon(const String& entity_id) const {
  return lookupKeyValue(data.entity_icons_map, entity_id);
}

String HaBridgeConfig::findSceneEntity(const String& alias) const {
  return lookupKeyValue(data.scene_alias_text, alias);
}

String HaBridgeConfig::buildJsonPayload(const char* device_id,
                                        const char* base_topic,
                                        const char* ha_prefix) const {
  if (!device_id || !*device_id || !base_topic || !*base_topic || !ha_prefix || !*ha_prefix) {
    return String();
  }

  String json = "{";
  json += "\"device_id\":\"";
  appendJsonEscaped(json, device_id);
  json += "\",\"base_topic\":\"";
  appendJsonEscaped(json, base_topic);
  json += "\",\"ha_prefix\":\"";
  appendJsonEscaped(json, ha_prefix);
  json += "\",\"sensors\":";
  appendSensorsJson(json, data.sensors_text);

  json += ",\"scene_map\":";
  appendSceneMapJson(json, data.scene_alias_text);
  json += "}";
  return json;
}

void HaBridgeConfig::appendJsonEscaped(String& out, const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '\"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
}

String HaBridgeConfig::normalizeLine(const String& line) {
  String t = line;
  t.replace("\r", "");
  t.trim();
  return t;
}

void HaBridgeConfig::appendSensorsJson(String& out, const String& text) {
  out += "[";
  bool first = true;
  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String entry = normalizeLine(text.substring(start, end));
    if (entry.length()) {
      if (!first) out += ",";
      out += "\"";
      appendJsonEscaped(out, entry);
      out += "\"";
      first = false;
    }
    start = end + 1;
  }
  out += "]";
}

void HaBridgeConfig::appendSceneMapJson(String& out, const String& text) {
  out += "{";
  bool first = true;
  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String entry = normalizeLine(text.substring(start, end));
    if (entry.length()) {
      int eq = entry.indexOf('=');
      if (eq > 0) {
        String alias = entry.substring(0, eq);
        String scene = entry.substring(eq + 1);
        alias.trim();
        scene.trim();
        alias.toLowerCase();
        if (alias.length() && scene.length()) {
          if (!first) out += ",";
          out += "\"";
          appendJsonEscaped(out, alias);
          out += "\":\"";
          appendJsonEscaped(out, scene);
          out += "\"";
          first = false;
        }
      }
    }
    start = end + 1;
  }
  out += "}";
}

static void parseArraySection(const String& body, String& out) {
  int start = body.indexOf('[');
  int end = body.indexOf(']', start);
  if (start < 0 || end < start) {
    out = "";
    return;
  }
  String result;
  String list = body.substring(start + 1, end);
  int pos = 0;
  while (pos < list.length()) {
    int q1 = list.indexOf('"', pos);
    if (q1 < 0) break;
    int q2 = list.indexOf('"', q1 + 1);
    if (q2 < 0) break;
    String value = list.substring(q1 + 1, q2);
    value.trim();
    if (value.length()) {
      if (result.length()) result += '\n';
      result += value;
    }
    pos = q2 + 1;
  }
  out = result;
}

static void parseObjectSection(const String& body, String& out) {
  int start = body.indexOf('{');
  int end = findMatchingJsonObjectEnd(body, start);
  if (start < 0 || end < start) {
    out = "";
    return;
  }
  String result;
  String obj = body.substring(start + 1, end);
  int pos = 0;
  while (pos < obj.length()) {
    int k1 = obj.indexOf('"', pos);
    if (k1 < 0) break;
    int k2 = obj.indexOf('"', k1 + 1);
    if (k2 < 0) break;
    String key = obj.substring(k1 + 1, k2);
    int colon = obj.indexOf(':', k2);
    if (colon < 0) break;
    int v1 = obj.indexOf('"', colon);
    if (v1 < 0) break;
    int v2 = obj.indexOf('"', v1 + 1);
    if (v2 < 0) break;
    String value = obj.substring(v1 + 1, v2);
    key.trim();
    value.trim();
    if (key.length() && value.length()) {
      if (result.length()) result += '\n';
      result += key + "=" + value;
    }
    pos = v2 + 1;
  }
  out = result;
}

static int findMatchingJsonObjectEnd(const String& body, int object_start) {
  if (object_start < 0 || object_start >= body.length() || body.charAt(object_start) != '{') {
    return -1;
  }

  int depth = 0;
  bool in_string = false;
  bool escaped = false;

  for (int i = object_start; i < body.length(); ++i) {
    char c = body.charAt(i);

    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }

    if (c == '"') {
      in_string = true;
    } else if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) return i;
      if (depth < 0) return -1;
    }
  }

  return -1;
}

bool HaBridgeConfig::applyJson(const char* json_payload, bool* out_reload, bool* out_icons_changed) {
  if (out_reload) {
    *out_reload = false;
  }
  if (out_icons_changed) {
    *out_icons_changed = false;
  }
  if (!json_payload || !*json_payload) {
    return false;
  }

  String json = json_payload;
  HaBridgeConfigData merged = data;

  int sensors_idx = json.indexOf("\"sensors\"");
  if (sensors_idx >= 0) {
    parseArraySection(json.substring(sensors_idx), merged.sensors_text);
  }

  int energy_idx = json.indexOf("\"energy\"");
  if (energy_idx < 0) {
    merged.energy_text = "";
  }

  int weathers_idx = json.indexOf("\"weathers\"");
  if (weathers_idx >= 0) {
    parseArraySection(json.substring(weathers_idx), merged.weathers_text);
  }

  int lights_idx = json.indexOf("\"lights\"");
  if (lights_idx >= 0) {
    parseArraySection(json.substring(lights_idx), merged.lights_text);
  }

  int switches_idx = json.indexOf("\"switches\"");
  if (switches_idx >= 0) {
    parseArraySection(json.substring(switches_idx), merged.switches_text);
  }

  int media_players_idx = json.indexOf("\"media_players\"");
  if (media_players_idx >= 0) {
    parseArraySection(json.substring(media_players_idx), merged.media_players_text);
  } else {
    merged.media_players_text = "";
  }

  int scene_idx = json.indexOf("\"scene_map\"");
  if (scene_idx >= 0) {
    parseObjectSection(json.substring(scene_idx), merged.scene_alias_text);
  }

  const String prev_icons = data.entity_icons_map;
  parseSensorMetaSection(json, merged.sensor_units_map, merged.sensor_names_map, merged.sensor_values_map);
  parseEntityNameSection(json, "media_player_meta", merged.sensor_names_map);
  parseIconMetaSections(json, merged.entity_icons_map);
  if (!merged.entity_icons_map.length() && prev_icons.length()) {
    merged.entity_icons_map = prev_icons;
  }
  if (energy_idx >= 0) {
    parseEnergySection(json.substring(energy_idx),
                       merged.energy_text,
                       merged.sensor_units_map,
                       merged.sensor_names_map,
                       merged.entity_icons_map);
  }
  bool icon_map_changed = !mapEqualsIgnoringOrder(prev_icons, merged.entity_icons_map);
  if (out_icons_changed) {
    *out_icons_changed = icon_map_changed;
  }

  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    if (merged.sensor_slots[i].length() &&
        !sensorExistsInList(merged.sensors_text, merged.sensor_slots[i])) {
      merged.sensor_slots[i] = "";
      merged.sensor_titles[i] = "";
      merged.sensor_custom_units[i] = "";
    }
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    if (merged.scene_slots[i].length() &&
        !aliasExistsInList(merged.scene_alias_text, merged.scene_slots[i])) {
      merged.scene_slots[i] = "";
      merged.scene_titles[i] = "";
    }
  }

  bool needs_reload = !bridgeConfigEquals(merged, data);
  if (needs_reload) {
    bool ok = save(merged);
    if (ok) {
      Serial.println("[Bridge] Konfiguration aus Home Assistant uebernommen");
      logList("Sensoren", data.sensors_text);
      logList("Energy", data.energy_text);
      logList("Wetter", data.weathers_text);
      logList("Lichter", data.lights_text);
      logList("Schalter", data.switches_text);
      logList("Media Player", data.media_players_text);
      logList("Szenen", data.scene_alias_text);
      if (out_reload) {
        *out_reload = true;
      }
    }
    return ok;
  }

  // No structural change: keep the latest meta in memory without triggering a reload.
  data = merged;
  return true;
}

bool HaBridgeConfig::applyIconUpdate(const char* json_payload) {
  if (!json_payload || !*json_payload) return false;

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, json_payload);
  if (err) return false;

  JsonObject obj = doc.as<JsonObject>();
  if (obj.isNull()) return false;

  bool changed = false;
  for (JsonPair kv : obj) {
    const char* entity_key = kv.key().c_str();
    if (!entity_key || !*entity_key) continue;

    String entity_id(entity_key);
    String next_icon;
    if (!kv.value().isNull()) {
      const char* icon = kv.value().as<const char*>();
      if (icon) {
        next_icon = icon;
        next_icon.trim();
      }
    }

    String current_icon = lookupKeyValue(data.entity_icons_map, entity_id);
    if (!next_icon.length()) {
      if (current_icon.length() && removeKeyValueMapEntry(data.entity_icons_map, entity_id)) {
        changed = true;
      }
      continue;
    }

    if (!current_icon.equals(next_icon)) {
      upsertKeyValueMap(data.entity_icons_map, entity_id, next_icon);
      changed = true;
    }
  }
  return changed;
}

static void logList(const char* label, const String& text) {
  if (!label) label = "Liste";
  if (!text.length()) {
    Serial.printf("[Bridge] %s: (leer)\n", label);
    return;
  }
  Serial.printf("[Bridge] %s:\n", label);
  int start = 0;
  int idx = 1;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    line.trim();
    if (line.length()) {
      Serial.printf("  %d) %s\n", idx++, line.c_str());
    }
    start = end + 1;
  }
}

static bool sensorExistsInList(const String& list, const String& candidate) {
  if (!candidate.length()) return true;
  int start = 0;
  while (start < list.length()) {
    int end = list.indexOf('\n', start);
    if (end < 0) end = list.length();
    String line = list.substring(start, end);
    line.trim();
    if (line.equalsIgnoreCase(candidate)) {
      return true;
    }
    start = end + 1;
  }
  return false;
}

static bool aliasExistsInList(const String& list, const String& alias) {
  if (!alias.length()) return true;
  int start = 0;
  while (start < list.length()) {
    int end = list.indexOf('\n', start);
    if (end < 0) end = list.length();
    String line = list.substring(start, end);
    int eq = line.indexOf('=');
    if (eq > 0) {
      String key = line.substring(0, eq);
      key.trim();
      key.toLowerCase();
      if (key == alias || key.equalsIgnoreCase(alias)) {
        return true;
      }
    }
    start = end + 1;
  }
  return false;
}

static String normalizeLineLocal(const String& line) {
  String t = line;
  t.replace("\r", "");
  t.trim();
  return t;
}

static int countListEntries(const String& text) {
  int count = 0;
  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = normalizeLineLocal(text.substring(start, end));
    if (line.length()) {
      ++count;
    }
    start = end + 1;
  }
  return count;
}

static int countMapEntries(const String& text) {
  int count = 0;
  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = normalizeLineLocal(text.substring(start, end));
    if (line.length()) {
      int eq = line.indexOf('=');
      if (eq > 0) {
        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);
        key.trim();
        value.trim();
        if (key.length() && value.length()) {
          ++count;
        }
      }
    }
    start = end + 1;
  }
  return count;
}

static bool listEqualsIgnoringOrder(const String& a, const String& b) {
  if (countListEntries(a) != countListEntries(b)) return false;

  int start = 0;
  while (start < a.length()) {
    int end = a.indexOf('\n', start);
    if (end < 0) end = a.length();
    String line = normalizeLineLocal(a.substring(start, end));
    if (line.length() && !sensorExistsInList(b, line)) {
      return false;
    }
    start = end + 1;
  }

  start = 0;
  while (start < b.length()) {
    int end = b.indexOf('\n', start);
    if (end < 0) end = b.length();
    String line = normalizeLineLocal(b.substring(start, end));
    if (line.length() && !sensorExistsInList(a, line)) {
      return false;
    }
    start = end + 1;
  }

  return true;
}

static bool mapEqualsIgnoringOrder(const String& a, const String& b) {
  if (countMapEntries(a) != countMapEntries(b)) return false;

  int start = 0;
  while (start < a.length()) {
    int end = a.indexOf('\n', start);
    if (end < 0) end = a.length();
    String line = normalizeLineLocal(a.substring(start, end));
    if (line.length()) {
      int eq = line.indexOf('=');
      if (eq > 0) {
        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);
        key.trim();
        value.trim();
        if (key.length()) {
          String other = lookupKeyValue(b, key);
          if (!other.equals(value)) {
            return false;
          }
        }
      }
    }
    start = end + 1;
  }

  start = 0;
  while (start < b.length()) {
    int end = b.indexOf('\n', start);
    if (end < 0) end = b.length();
    String line = normalizeLineLocal(b.substring(start, end));
    if (line.length()) {
      int eq = line.indexOf('=');
      if (eq > 0) {
        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);
        key.trim();
        value.trim();
        if (key.length()) {
          String other = lookupKeyValue(a, key);
          if (!other.equals(value)) {
            return false;
          }
        }
      }
    }
    start = end + 1;
  }

  return true;
}

static bool bridgeConfigEquals(const HaBridgeConfigData& a, const HaBridgeConfigData& b) {
  if (!listEqualsIgnoringOrder(a.sensors_text, b.sensors_text)) return false;
  if (!listEqualsIgnoringOrder(a.energy_text, b.energy_text)) return false;
  if (!listEqualsIgnoringOrder(a.weathers_text, b.weathers_text)) return false;
  if (!listEqualsIgnoringOrder(a.lights_text, b.lights_text)) return false;
  if (!listEqualsIgnoringOrder(a.switches_text, b.switches_text)) return false;
  if (!listEqualsIgnoringOrder(a.media_players_text, b.media_players_text)) return false;
  if (!mapEqualsIgnoringOrder(a.scene_alias_text, b.scene_alias_text)) return false;

  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    if (!a.sensor_slots[i].equals(b.sensor_slots[i])) return false;
    if (!a.sensor_titles[i].equals(b.sensor_titles[i])) return false;
    if (!a.sensor_custom_units[i].equals(b.sensor_custom_units[i])) return false;
    if (a.sensor_colors[i] != b.sensor_colors[i]) return false;
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    if (!a.scene_slots[i].equals(b.scene_slots[i])) return false;
    if (!a.scene_titles[i].equals(b.scene_titles[i])) return false;
    if (a.scene_colors[i] != b.scene_colors[i]) return false;
  }
  return true;
}

static bool isHexDigit(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static uint8_t hexValue(char c) {
  if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
  if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
  if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
  return 0;
}

static void appendUtf8(String& out, uint32_t codepoint) {
  if (codepoint <= 0x7F) {
    out += static_cast<char>(codepoint);
  } else if (codepoint <= 0x7FF) {
    out += static_cast<char>(0xC0 | (codepoint >> 6));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0xFFFF) {
    out += static_cast<char>(0xE0 | (codepoint >> 12));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (codepoint >> 18));
    out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  }
}

static String decodeJsonEscapes(const String& value) {
  if (!value.length()) return value;
  String out;
  out.reserve(value.length());
  const size_t len = value.length();

  for (size_t i = 0; i < len; ++i) {
    char c = value.charAt(i);
    if (c != '\\' || i + 1 >= len) {
      out += c;
      continue;
    }

    char esc = value.charAt(i + 1);
    if (esc == 'u' && (i + 5) < len) {
      if (isHexDigit(value.charAt(i + 2)) &&
          isHexDigit(value.charAt(i + 3)) &&
          isHexDigit(value.charAt(i + 4)) &&
          isHexDigit(value.charAt(i + 5))) {
        uint16_t code = (hexValue(value.charAt(i + 2)) << 12) |
                        (hexValue(value.charAt(i + 3)) << 8) |
                        (hexValue(value.charAt(i + 4)) << 4) |
                        (hexValue(value.charAt(i + 5)));

        i += 5;

        if (code >= 0xD800 && code <= 0xDBFF) {
          if ((i + 6) < len && value.charAt(i + 1) == '\\' && value.charAt(i + 2) == 'u' &&
              isHexDigit(value.charAt(i + 3)) && isHexDigit(value.charAt(i + 4)) &&
              isHexDigit(value.charAt(i + 5)) && isHexDigit(value.charAt(i + 6))) {
            uint16_t low = (hexValue(value.charAt(i + 3)) << 12) |
                           (hexValue(value.charAt(i + 4)) << 8) |
                           (hexValue(value.charAt(i + 5)) << 4) |
                           (hexValue(value.charAt(i + 6)));
            if (low >= 0xDC00 && low <= 0xDFFF) {
              uint32_t codepoint = 0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
              appendUtf8(out, codepoint);
              i += 6;
              continue;
            }
          }
        }

        appendUtf8(out, code);
        continue;
      }
    }

    switch (esc) {
      case '\"': out += '\"'; break;
      case '\\': out += '\\'; break;
      case '/': out += '/'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      default: out += esc; break;
    }
    i += 1;
  }

  return out;
}

static bool extractStringField(const String& object, const char* key, String& out) {
  if (!key || !*key) return false;
  String pattern = String("\"") + key + "\"";
  int idx = object.indexOf(pattern);
  if (idx < 0) return false;
  int colon = object.indexOf(':', idx);
  if (colon < 0) return false;
  int q1 = object.indexOf('"', colon);
  if (q1 < 0) return false;
  int q2 = object.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  out = object.substring(q1 + 1, q2);
  out = decodeJsonEscapes(out);
  out.trim();
  return out.length() > 0;
}

static const char* energyIconForCategory(const String& category, const String& id, const String& unit) {
  String cat = category;
  cat.trim();
  cat.toLowerCase();
  String entity = id;
  entity.toLowerCase();
  String u = unit;
  u.toLowerCase();

  if (entity.endsWith("_cost") || u == "eur" || u == "euro") return "currency-eur";
  if (cat == "solar") return "solar-power";
  if (cat == "grid") return "transmission-tower";
  if (cat == "battery") return "battery-charging";
  if (cat == "gas") return "fire";
  if (cat == "water" || cat == "device_water") return "water";
  return "lightning-bolt";
}

static void appendUniqueListEntry(String& list, const String& entry) {
  if (!entry.length()) return;
  if (sensorExistsInList(list, entry)) return;
  if (list.length()) list += '\n';
  list += entry;
}

static void parseEnergySection(const String& body,
                               String& energy,
                               String& units,
                               String& names,
                               String& icons) {
  energy = "";
  int energy_idx = body.indexOf("\"energy\"");
  int array_start = body.indexOf('[', energy_idx >= 0 ? energy_idx : 0);
  int array_end = body.indexOf(']', array_start);
  if (array_start < 0 || array_end < array_start) {
    return;
  }

  String segment = body.substring(array_start + 1, array_end);
  int obj_start = segment.indexOf('{');
  while (obj_start >= 0) {
    int obj_end = segment.indexOf('}', obj_start);
    if (obj_end < 0) break;

    String object = segment.substring(obj_start, obj_end + 1);
    String id;
    if (extractStringField(object, "id", id)) {
      appendUniqueListEntry(energy, id);

      String name;
      if (extractStringField(object, "name", name)) {
        upsertKeyValueMap(names, id, name);
      }

      String unit;
      if (extractStringField(object, "unit", unit)) {
        upsertKeyValueMap(units, id, unit);
      }

      String category;
      extractStringField(object, "category", category);
      upsertKeyValueMap(icons, id, energyIconForCategory(category, id, unit));
    }

    obj_start = segment.indexOf('{', obj_end + 1);
  }
}

static void parseSensorMetaSection(const String& body, String& units, String& names, String& values) {
  units = "";
  names = "";
  values = "";
  int meta_idx = body.indexOf("\"sensor_meta\"");
  if (meta_idx < 0) {
    return;
  }
  int array_start = body.indexOf('[', meta_idx);
  int array_end = body.indexOf(']', array_start);
  if (array_start < 0 || array_end < array_start) {
    return;
  }
  String segment = body.substring(array_start + 1, array_end);
  int obj_start = segment.indexOf('{');
  while (obj_start >= 0) {
    int obj_end = segment.indexOf('}', obj_start);
    if (obj_end < 0) break;
    String object = segment.substring(obj_start, obj_end + 1);
    String entity;
    if (!extractStringField(object, "entity_id", entity)) {
      obj_start = segment.indexOf('{', obj_end + 1);
      continue;
    }
    String unit;
    if (extractStringField(object, "unit", unit)) {
      if (units.length()) units += '\n';
      units += entity + "=" + unit;
    }
    String name;
    if (extractStringField(object, "name", name)) {
      if (names.length()) names += '\n';
      names += entity + "=" + name;
    }
    String value;
    if (extractStringField(object, "value", value)) {
      if (values.length()) values += '\n';
      values += entity + "=" + value;
    }
    obj_start = segment.indexOf('{', obj_end + 1);
  }
}

static void parseEntityNameSection(const String& body, const char* key, String& names) {
  if (!key || !*key) return;
  String pattern = String("\"") + key + "\"";
  int meta_idx = body.indexOf(pattern);
  if (meta_idx < 0) return;
  int array_start = body.indexOf('[', meta_idx);
  int array_end = body.indexOf(']', array_start);
  if (array_start < 0 || array_end < array_start) return;

  String segment = body.substring(array_start + 1, array_end);
  int obj_start = segment.indexOf('{');
  while (obj_start >= 0) {
    int obj_end = segment.indexOf('}', obj_start);
    if (obj_end < 0) break;
    String object = segment.substring(obj_start, obj_end + 1);
    String entity;
    String name;
    if (extractStringField(object, "entity_id", entity) &&
        extractStringField(object, "name", name)) {
      upsertKeyValueMap(names, entity, name);
    }
    obj_start = segment.indexOf('{', obj_end + 1);
  }
}

static void parseEntityIconSection(const String& body, const char* key, String& icons) {
  if (!key || !*key) return;
  String pattern = String("\"") + key + "\"";
  int meta_idx = body.indexOf(pattern);
  if (meta_idx < 0) {
    return;
  }
  int array_start = body.indexOf('[', meta_idx);
  int array_end = body.indexOf(']', array_start);
  if (array_start < 0 || array_end < array_start) {
    return;
  }
  String segment = body.substring(array_start + 1, array_end);
  int obj_start = segment.indexOf('{');
  while (obj_start >= 0) {
    int obj_end = segment.indexOf('}', obj_start);
    if (obj_end < 0) break;
    String object = segment.substring(obj_start, obj_end + 1);
    String entity;
    if (!extractStringField(object, "entity_id", entity)) {
      obj_start = segment.indexOf('{', obj_end + 1);
      continue;
    }
    String icon;
    if (extractStringField(object, "icon", icon)) {
      if (icons.length()) icons += '\n';
      icons += entity + "=" + icon;
    }
    obj_start = segment.indexOf('{', obj_end + 1);
  }
}

static void parseIconMetaSections(const String& body, String& icons) {
  icons = "";
  parseEntityIconSection(body, "sensor_meta", icons);
  parseEntityIconSection(body, "weather_meta", icons);
  parseEntityIconSection(body, "light_meta", icons);
  parseEntityIconSection(body, "switch_meta", icons);
  parseEntityIconSection(body, "scene_meta", icons);
  parseEntityIconSection(body, "media_player_meta", icons);
}

static String lookupKeyValue(const String& text, const String& key) {
  if (!key.length()) return "";
  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    int eq = line.indexOf('=');
    if (eq > 0) {
      String lhs = line.substring(0, eq);
      lhs.trim();
      if (lhs.equalsIgnoreCase(key)) {
        String rhs = line.substring(eq + 1);
        rhs.trim();
        return rhs;
      }
    }
    start = end + 1;
  }
  return "";
}

static void upsertKeyValueMap(String& text, const String& key, const String& value) {
  if (!key.length() || !value.length()) return;

  String newMap = "";
  bool found = false;

  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();

    String line = text.substring(start, end);
    int eq = line.indexOf('=');

    if (eq > 0) {
      String entity = line.substring(0, eq);
      entity.trim();

      if (entity.equalsIgnoreCase(key)) {
        if (newMap.length()) newMap += '\n';
        newMap += key + "=" + value;
        found = true;
      } else {
        if (newMap.length()) newMap += '\n';
        newMap += line;
      }
    } else if (line.length() > 0) {
      if (newMap.length()) newMap += '\n';
      newMap += line;
    }

    start = end + 1;
  }

  if (!found) {
    if (newMap.length()) newMap += '\n';
    newMap += key + "=" + value;
  }

  text = newMap;
}

static bool removeKeyValueMapEntry(String& text, const String& key) {
  if (!key.length() || !text.length()) return false;

  String newMap = "";
  bool removed = false;

  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();

    String line = text.substring(start, end);
    int eq = line.indexOf('=');
    String entity = eq > 0 ? line.substring(0, eq) : line;
    entity.trim();

    if (entity.equalsIgnoreCase(key)) {
      removed = true;
    } else if (line.length() > 0) {
      if (newMap.length()) newMap += '\n';
      newMap += line;
    }

    start = end + 1;
  }

  if (removed) {
    text = newMap;
  }
  return removed;
}

void HaBridgeConfig::registerSensorMeta(const String& entity_id, const String& name, const String& unit) {
  if (entity_id.length() == 0) return;

  if (!sensorExistsInList(data.sensors_text, entity_id)) {
    if (data.sensors_text.length()) data.sensors_text += '\n';
    data.sensors_text += entity_id;
  }

  upsertKeyValueMap(data.sensor_names_map, entity_id, name);
  upsertKeyValueMap(data.sensor_units_map, entity_id, unit);
}

void HaBridgeConfig::updateEntityMeta(const String& entity_id, const String& name, const String& unit, const String& icon) {
  if (entity_id.length() == 0) return;
  if (name.length()) {
    upsertKeyValueMap(data.sensor_names_map, entity_id, name);
  }
  if (unit.length()) {
    upsertKeyValueMap(data.sensor_units_map, entity_id, unit);
  }
  if (icon.length()) {
    upsertKeyValueMap(data.entity_icons_map, entity_id, icon);
  }
}

void HaBridgeConfig::updateSensorValue(const String& entity_id, const String& value) {
  if (entity_id.length() == 0) return;

  // Parse sensor_values_map and update/add the value
  String& valuesMap = data.sensor_values_map;
  String newMap = "";
  bool found = false;

  int start = 0;
  while (start < valuesMap.length()) {
    int end = valuesMap.indexOf('\n', start);
    if (end < 0) end = valuesMap.length();

    String line = valuesMap.substring(start, end);
    int eq = line.indexOf('=');

    if (eq > 0) {
      String entity = line.substring(0, eq);
      entity.trim();

      if (entity.equalsIgnoreCase(entity_id)) {
        // Update existing entry
        if (newMap.length()) newMap += '\n';
        newMap += entity_id + "=" + value;
        found = true;
      } else {
        // Keep existing entry
        if (newMap.length()) newMap += '\n';
        newMap += line;
      }
    } else if (line.length() > 0) {
      // Keep non-empty lines without '='
      if (newMap.length()) newMap += '\n';
      newMap += line;
    }

    start = end + 1;
  }

  // Add new entry if not found
  if (!found) {
    if (newMap.length()) newMap += '\n';
    newMap += entity_id + "=" + value;
  }

  valuesMap = newMap;
}
