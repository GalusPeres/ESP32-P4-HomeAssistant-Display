#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "src/devices/device.h"
#include "src/core/hardware/board_hal.h"
#include "src/core/display/display_manager.h"

// Stateless helpers shared by HTTP handler translation units.
// Internal inline linkage keeps the original per-call optimization available.
namespace {

inline bool endsWithIgnoreCase(const String& value, const char* suffix) {
  if (!suffix) return false;
  String v = value;
  v.toLowerCase();
  String s = suffix;
  s.toLowerCase();
  return v.endsWith(s);
}

inline bool storageReady() {
  return Device::storageReady();
}

inline fs::FS& storageFS() {
  return Device::storageFS();
}

inline bool sdReady() {
  return Device::sdReady();
}

inline fs::FS& sdFS() {
  return Device::sdFS();
}

inline void appendJsonEscaped(String& out, const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (c == '\"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out += c;
    }
  }
}

inline void sendJsonError(WebServer& server, int code, const String& error) {
  String json = "{\"success\":false,\"error\":\"";
  appendJsonEscaped(json, error);
  json += "\"}";
  server.send(code, "application/json", json);
}

inline void prepareDisplayForRestart() {
  displayManager.setInputEnabled(false);
  BoardHAL::prepareForRestart();
}

}  // namespace
