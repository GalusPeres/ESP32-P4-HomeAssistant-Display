#include "src/types/binary_sensor/web_scripts.h"

#include "src/core/config/config_manager.h"
#include "src/core/i18n/i18n.h"

namespace {

void append_binary_sensor_js_string(String& html, const String& value) {
  html += "'";
  for (size_t index = 0; index < value.length(); ++index) {
    const char c = value[index];
    switch (c) {
      case '\\': html += "\\\\"; break;
      case '\'': html += "\\'"; break;
      case '\r': break;
      case '\n': html += "\\n"; break;
      case '<': html += "\\x3c"; break;
      default: html += c; break;
    }
  }
  html += "'";
}

void append_binary_sensor_state_pair(String& html, const char* language,
                                     const char* device_class,
                                     bool& first) {
  if (!first) html += ",\n";
  first = false;
  html += "      ";
  append_binary_sensor_js_string(html, device_class);
  html += ": Object.freeze({on: ";
  append_binary_sensor_js_string(
      html, i18n::binary_sensor_state_label(language, "on", device_class));
  html += ", off: ";
  append_binary_sensor_js_string(
      html, i18n::binary_sensor_state_label(language, "off", device_class));
  html += "})";
}

}  // namespace

void append_binary_sensor_scripts(String& html) {
  const char* language = configManager.getConfig().language;
  static constexpr const char* kDeviceClasses[] = {
      "",          "battery",      "battery_charging", "carbon_monoxide",
      "cold",      "connectivity", "door",             "garage_door",
      "gas",       "heat",         "light",            "lock",
      "moisture",  "motion",       "moving",           "occupancy",
      "opening",   "plug",         "power",            "presence",
      "problem",   "running",      "safety",           "smoke",
      "sound",     "tamper",       "update",           "vibration",
      "window"};

  html += R"html(
  <script>
  const BINARY_SENSOR_I18N = Object.freeze({
    states: Object.freeze({
)html";
  bool first = true;
  for (const char* device_class : kDeviceClasses) {
    append_binary_sensor_state_pair(
        html, language, device_class, first);
  }
  html += R"html(
    }),
    unavailable: )html";
  append_binary_sensor_js_string(
      html, i18n::binary_sensor_state_label(
                language, "unavailable", ""));
  html += R"html(,
    unknown: )html";
  append_binary_sensor_js_string(
      html, i18n::binary_sensor_state_label(language, "unknown", ""));
  html += R"html(
  });
  </script>
)html";
}
