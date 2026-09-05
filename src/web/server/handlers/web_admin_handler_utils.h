#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "src/devices/device.h"
#include "src/core/hardware/board_hal.h"
#include "src/core/display/display_manager.h"

// Stateless helpers shared by HTTP handler translation units.
// Nontrivial helpers have one compiled owner; small hardware accessors stay inline.
namespace web_admin_handlers {

bool endsWithIgnoreCase(const String& value, const char* suffix);

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

void appendJsonEscaped(String& out, const String& value);

void sendJsonError(WebServer& server, int code, const String& error);

inline void prepareDisplayForRestart() {
  displayManager.setInputEnabled(false);
  BoardHAL::prepareForRestart();
}

}  // namespace web_admin_handlers
