#ifndef BATTERY_STATE_H
#define BATTERY_STATE_H

#include <Arduino.h>

struct BatteryTelemetry {
  bool on_mains = false;
  bool battery_missing = false;
  bool level_valid = false;
  int32_t level_pct = -1;
  int32_t raw_level_pct = -1;
  int32_t voltage_mv = 0;
  int32_t current_ma = 0;
  int16_t vbus_mv = -1;
  bool has_vbus = false;
  bool charging = false;
};

void batteryStateUpdate();
const BatteryTelemetry& batteryStateGet();
bool batteryStateIsOnMains();
bool batteryStateIsBatteryMissing();
bool batteryStateHasDisplayPercent();
int32_t batteryStateDisplayPercent();
int32_t batteryStateRawPercent();

#endif // BATTERY_STATE_H
