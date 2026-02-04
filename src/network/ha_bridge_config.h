#ifndef HA_BRIDGE_CONFIG_H
#define HA_BRIDGE_CONFIG_H

#include <Arduino.h>

static constexpr size_t HA_SENSOR_SLOT_COUNT = 6;
static constexpr size_t HA_SCENE_SLOT_COUNT = 6;

struct HaBridgeConfigData {
  String sensors_text;
  String weathers_text;
  String lights_text;
  String switches_text;
  String scene_alias_text;
  String sensor_slots[HA_SENSOR_SLOT_COUNT];
  String scene_slots[HA_SCENE_SLOT_COUNT];
  String sensor_units_map;
  String sensor_names_map;
  String sensor_values_map;
  String entity_icons_map;
  String sensor_titles[HA_SENSOR_SLOT_COUNT];
  String sensor_custom_units[HA_SENSOR_SLOT_COUNT];
  String scene_titles[HA_SCENE_SLOT_COUNT];
  uint32_t sensor_colors[HA_SENSOR_SLOT_COUNT];  // RGB Hex (0 = Standard 0x2A2A2A)
  uint32_t scene_colors[HA_SCENE_SLOT_COUNT];    // RGB Hex (0 = Standard 0x353535)
};

class HaBridgeConfig {
public:
  HaBridgeConfig();

  bool load();
  bool save(const HaBridgeConfigData& data);
  bool applyJson(const char* json_payload, bool* out_reload = nullptr, bool* out_icons_changed = nullptr);

  const HaBridgeConfigData& get() const { return data; }
  bool hasData() const;
  String findSensorUnit(const String& entity_id) const;
  String findSensorName(const String& entity_id) const;
  String findSensorInitialValue(const String& entity_id) const;
  String findEntityIcon(const String& entity_id) const;
  String findSceneEntity(const String& alias) const;

  // Update live sensor value (for web interface)
  void updateSensorValue(const String& entity_id, const String& value);
  void registerSensorMeta(const String& entity_id, const String& name, const String& unit);

  String buildJsonPayload(const char* device_id,
                          const char* base_topic,
                          const char* ha_prefix) const;

private:
  HaBridgeConfigData data;

  static void appendJsonEscaped(String& out, const String& value);
  static void appendSensorsJson(String& out, const String& text);
  static void appendSceneMapJson(String& out, const String& text);
  static String normalizeLine(const String& line);
};

extern HaBridgeConfig haBridgeConfig;

#endif // HA_BRIDGE_CONFIG_H
