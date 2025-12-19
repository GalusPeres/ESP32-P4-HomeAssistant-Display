#include "src/network/ha_bridge_config.h"

#include <Preferences.h>
#include <stdio.h>

static const char* PREF_NAMESPACE = "tab5_config";
static void logList(const char* label, const String& text);
static bool sensorExistsInList(const String& list, const String& candidate);
static bool aliasExistsInList(const String& list, const String& alias);
static void parseSensorMetaSection(const String& body, String& units, String& names, String& values);
static bool extractStringField(const String& object, const char* key, String& out);
static String lookupKeyValue(const String& text, const String& key);

HaBridgeConfig haBridgeConfig;

HaBridgeConfig::HaBridgeConfig() = default;

bool HaBridgeConfig::load() {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    return false;
  }

  data.sensors_text = prefs.getString("ha_sensors", "");
  data.lights_text = "";
  data.switches_text = "";
  data.scene_alias_text = prefs.getString("ha_scene_alias", "");
  data.sensor_units_map = prefs.getString("ha_sens_units", "");
  data.sensor_names_map = prefs.getString("ha_sens_names", "");
  data.sensor_values_map = prefs.getString("ha_sens_vals", "");
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
         data.lights_text.length() > 0 ||
         data.switches_text.length() > 0 ||
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
  int end = body.lastIndexOf('}');
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

bool HaBridgeConfig::applyJson(const char* json_payload) {
  if (!json_payload || !*json_payload) {
    return false;
  }

  String json = json_payload;
  HaBridgeConfigData merged = data;

  int sensors_idx = json.indexOf("\"sensors\"");
  if (sensors_idx >= 0) {
    parseArraySection(json.substring(sensors_idx), merged.sensors_text);
  }

  int lights_idx = json.indexOf("\"lights\"");
  if (lights_idx >= 0) {
    parseArraySection(json.substring(lights_idx), merged.lights_text);
  }

  int switches_idx = json.indexOf("\"switches\"");
  if (switches_idx >= 0) {
    parseArraySection(json.substring(switches_idx), merged.switches_text);
  }

  int scene_idx = json.indexOf("\"scene_map\"");
  if (scene_idx >= 0) {
    parseObjectSection(json.substring(scene_idx), merged.scene_alias_text);
  }

  parseSensorMetaSection(json, merged.sensor_units_map, merged.sensor_names_map, merged.sensor_values_map);

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

  bool ok = save(merged);
  if (ok) {
    Serial.println("[Bridge] Konfiguration aus Home Assistant uebernommen");
    logList("Sensoren", data.sensors_text);
    logList("Lichter", data.lights_text);
    logList("Schalter", data.switches_text);
    logList("Szenen", data.scene_alias_text);
  }
  return ok;
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
  out.trim();
  return out.length() > 0;
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
