#include "src/core/battery_state.h"
#include <M5Unified.h>

namespace {

static constexpr int32_t kCurrentToMainsMinMa = 40;
static constexpr int32_t kCurrentToBatteryMaxMa = -40;
static constexpr int16_t kVbusPresentMinMv = 4300;

static constexpr uint32_t kPowerStateDebounceMs = 1200;
static constexpr uint32_t kBatteryMissingDebounceMs = 3000;
static constexpr uint32_t kBatteryFreezeAfterPlugMs = 10000;
static constexpr uint32_t kBatteryDropStepMs = 120000;          // on battery: max -1% every 2 min
static constexpr uint32_t kBatteryRiseOnBatteryStepMs = 300000; // on battery: max +1% every 5 min
static constexpr uint32_t kBatteryRiseOnMainsStepMs = 20000;    // on mains: max +1% every 20 s

int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int32_t abs_i32(int32_t v) {
  return (v < 0) ? -v : v;
}

bool is_battery_missing_raw(bool on_mains, int32_t level_pct, int32_t current_ma) {
  // Tab5 can report undefined values when no NP-F battery is attached.
  // Treat "plugged + ~0% + almost no battery current" as battery missing.
  if (!on_mains) return false;
  if (level_pct > 1) return false;
  if (abs_i32(current_ma) > 120) return false;
  return true;
}

struct BatteryStateInternal {
  bool initialized = false;
  bool power_candidate = false;
  uint32_t power_candidate_since_ms = 0;
  bool missing_candidate = false;
  uint32_t missing_candidate_since_ms = 0;
  bool display_pct_initialized = false;
  int32_t display_pct = 0;
  uint32_t last_pct_adjust_ms = 0;
  uint32_t freeze_pct_until_ms = 0;
  BatteryTelemetry out;
};

BatteryStateInternal g_state;

bool detect_mains_raw(const BatteryStateInternal& state,
                      int32_t current_ma,
                      int16_t vbus_mv,
                      m5::Power_Class::is_charging_t charge_state) {
  if (vbus_mv >= 0) {
    if (vbus_mv >= kVbusPresentMinMv) return true;
    if (vbus_mv <= 1000) return false;
  }

  if (current_ma <= kCurrentToBatteryMaxMa) {
    return false;
  }
  if (current_ma >= kCurrentToMainsMinMa) {
    return true;
  }

  if (charge_state == m5::Power_Class::is_charging_t::is_discharging) {
    return false;
  }
  if (charge_state == m5::Power_Class::is_charging_t::is_charging) {
    return true;
  }

  if (state.initialized) {
    return state.out.on_mains;
  }

  return current_ma >= 0;
}

} // namespace

void batteryStateUpdate() {
  BatteryStateInternal& s = g_state;
  uint32_t now = millis();

  int32_t raw_level = M5.Power.getBatteryLevel();
  bool raw_level_valid = (raw_level >= 0 && raw_level <= 100);
  raw_level = clamp_i32(raw_level, 0, 100);

  int32_t raw_voltage = M5.Power.getBatteryVoltage();
  int32_t raw_current = M5.Power.getBatteryCurrent();
  int16_t raw_vbus = M5.Power.getVBUSVoltage();
  m5::Power_Class::is_charging_t charge_state = M5.Power.isCharging();
  bool charging = (charge_state == m5::Power_Class::is_charging_t::is_charging);

  bool mains_raw = detect_mains_raw(s, raw_current, raw_vbus, charge_state);

  if (!s.initialized) {
    s.out.on_mains = mains_raw;
    s.power_candidate = mains_raw;
    s.power_candidate_since_ms = now;
    s.freeze_pct_until_ms = now + kBatteryFreezeAfterPlugMs;
    s.initialized = true;
  } else {
    if (mains_raw != s.out.on_mains) {
      if (mains_raw != s.power_candidate) {
        s.power_candidate = mains_raw;
        s.power_candidate_since_ms = now;
      } else if ((now - s.power_candidate_since_ms) >= kPowerStateDebounceMs) {
        s.out.on_mains = s.power_candidate;
        s.freeze_pct_until_ms = now + kBatteryFreezeAfterPlugMs;
        Serial.printf("[BAT] power -> %s (I=%ldmA VBUS=%dmV CHG=%d)\n",
                      s.out.on_mains ? "MAINS" : "BATTERY",
                      static_cast<long>(raw_current),
                      static_cast<int>(raw_vbus),
                      static_cast<int>(charge_state));
      }
    } else {
      s.power_candidate = mains_raw;
      s.power_candidate_since_ms = now;
    }
  }

  bool missing_raw = is_battery_missing_raw(s.out.on_mains, raw_level, raw_current);
  if (!s.out.on_mains) {
    missing_raw = false;
  }

  if (s.missing_candidate_since_ms == 0) {
    s.out.battery_missing = missing_raw;
    s.missing_candidate = missing_raw;
    s.missing_candidate_since_ms = now;
  } else if (missing_raw != s.missing_candidate) {
    s.missing_candidate = missing_raw;
    s.missing_candidate_since_ms = now;
  } else if (s.missing_candidate != s.out.battery_missing &&
             (now - s.missing_candidate_since_ms) >= kBatteryMissingDebounceMs) {
    s.out.battery_missing = s.missing_candidate;
  }

  if (!s.display_pct_initialized && raw_level_valid) {
    s.display_pct = raw_level;
    s.display_pct_initialized = true;
    s.last_pct_adjust_ms = now;
  } else if (s.display_pct_initialized && raw_level_valid && !s.out.battery_missing) {
    bool frozen = ((int32_t)(now - s.freeze_pct_until_ms) < 0);
    if (!frozen) {
      int32_t diff = raw_level - s.display_pct;
      if (diff < 0) {
        if (!s.out.on_mains && (now - s.last_pct_adjust_ms) >= kBatteryDropStepMs) {
          s.display_pct -= 1;
          if (s.display_pct < raw_level) s.display_pct = raw_level;
          s.last_pct_adjust_ms = now;
        }
      } else if (diff > 0) {
        uint32_t step = s.out.on_mains ? kBatteryRiseOnMainsStepMs : kBatteryRiseOnBatteryStepMs;
        if ((now - s.last_pct_adjust_ms) >= step) {
          s.display_pct += 1;
          if (s.display_pct > raw_level) s.display_pct = raw_level;
          s.last_pct_adjust_ms = now;
        }
      }
    }
  }

  if (s.display_pct_initialized) {
    s.display_pct = clamp_i32(s.display_pct, 0, 100);
  }

  bool have_display_level = s.display_pct_initialized && !s.out.battery_missing;
  s.out.level_valid = have_display_level;
  s.out.level_pct = have_display_level ? s.display_pct : -1;
  s.out.raw_level_pct = raw_level_valid ? raw_level : -1;
  s.out.voltage_mv = raw_voltage;
  s.out.current_ma = raw_current;
  s.out.vbus_mv = raw_vbus;
  s.out.has_vbus = raw_vbus >= 0;
  s.out.charging = charging;
}

const BatteryTelemetry& batteryStateGet() {
  return g_state.out;
}

bool batteryStateIsOnMains() {
  return g_state.out.on_mains;
}

bool batteryStateIsBatteryMissing() {
  return g_state.out.battery_missing;
}

bool batteryStateHasDisplayPercent() {
  return g_state.out.level_valid;
}

int32_t batteryStateDisplayPercent() {
  return g_state.out.level_pct;
}

int32_t batteryStateRawPercent() {
  return g_state.out.raw_level_pct;
}
