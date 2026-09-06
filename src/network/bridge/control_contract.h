#pragma once

#include <cstring>

namespace ha_control {
inline bool hasDomain(const char* entity, const char* domain) {
  if (!entity || !domain) return false;
  const size_t length = std::strlen(domain);
  return std::strncmp(entity, domain, length) == 0 && entity[length] == '.' && entity[length + 1];
}

inline bool supportsSwitchTile(const char* entity) {
  return hasDomain(entity, "light") || hasDomain(entity, "switch") ||
         hasDomain(entity, "input_boolean") || hasDomain(entity, "automation") ||
         hasDomain(entity, "fan") || hasDomain(entity, "humidifier") ||
         hasDomain(entity, "remote") || hasDomain(entity, "siren");
}

inline bool supportsSceneTile(const char* entity) {
  return hasDomain(entity, "scene") || hasDomain(entity, "script") ||
         hasDomain(entity, "button") || hasDomain(entity, "input_button");
}
}  // namespace ha_control
