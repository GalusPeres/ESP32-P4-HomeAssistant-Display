#include "src/core/power/battery_state.h"

// The shared telemetry implementation currently returns "on mains, no battery"
// defaults for every profile. Device-specific battery telemetry is not wired here.

static BatteryTelemetry g_stub = {
  .on_mains = true,
  .battery_missing = true,
  .level_valid = false,
  .level_pct = -1,
  .raw_level_pct = -1,
  .voltage_mv = 0,
  .current_ma = 0,
  .vbus_mv = 5000,
  .has_vbus = true,
  .charging = false,
};

void batteryStateUpdate() {
  // nothing to poll
}

const BatteryTelemetry& batteryStateGet() {
  return g_stub;
}

bool batteryStateIsOnMains() {
  return true;
}

bool batteryStateIsBatteryMissing() {
  return true;
}

bool batteryStateHasDisplayPercent() {
  return false;
}

int32_t batteryStateDisplayPercent() {
  return -1;
}

int32_t batteryStateRawPercent() {
  return -1;
}
