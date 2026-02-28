#include "src/core/battery_state.h"
#include <M5Unified.h>

namespace {

static constexpr int32_t kCurrentMainsThresholdMa = 50;

static constexpr uint32_t kPowerStateDebounceMs = 1200;
static constexpr uint32_t kBatteryMissingDebounceMs = 3000;
static constexpr uint32_t kBatteryFreezeAfterPlugMs = 10000;
static constexpr uint32_t kBatteryDropStepMs = 120000;          // on battery: max -1% every 2 min
static constexpr uint32_t kBatteryRiseOnBatteryStepMs = 300000; // on battery: max +1% every 5 min
static constexpr uint32_t kBatteryRiseOnMainsStepMs = 20000;    // on mains: max +1% every 20 s
// Hardware reserve calibration:
// Latest measured behavior: device is effectively empty around ~3%.
static constexpr int32_t kSocReserveFloorPct = 3;
// Runtime-linearization calibration from measured discharge behavior:
// 00:30 100% -> 18:30 50% -> 20:20 7%(empty)
// This means raw 50% is only about ~9% usable runtime left.
static constexpr int32_t kSocRuntimeKneeRawPct = 50;
static constexpr int32_t kSocRuntimeKneeDisplayPct = 9;
// While charging around empty, avoid staying visually stuck at 0%.
static constexpr int32_t kSocChargingMinDisplayPct = 1;

int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int32_t abs_i32(int32_t v) {
  return (v < 0) ? -v : v;
}

int32_t map_soc_to_usable_percent(int32_t raw_pct) {
  raw_pct = clamp_i32(raw_pct, 0, 100);
  if (raw_pct <= kSocReserveFloorPct) return 0;
  if (raw_pct >= 100) return 100;

  // Piecewise linear runtime mapping:
  // [reserve..knee_raw] -> [0..knee_display]
  // [knee_raw..100]     -> [knee_display..100]
  if (raw_pct <= kSocRuntimeKneeRawPct) {
    const int32_t in_span = kSocRuntimeKneeRawPct - kSocReserveFloorPct;
    const int32_t out_span = kSocRuntimeKneeDisplayPct;
    if (in_span <= 0 || out_span <= 0) return 0;
    const int32_t scaled =
        ((raw_pct - kSocReserveFloorPct) * out_span + (in_span / 2)) / in_span;
    return clamp_i32(scaled, 0, 100);
  }

  const int32_t in_span = 100 - kSocRuntimeKneeRawPct;
  const int32_t out_span = 100 - kSocRuntimeKneeDisplayPct;
  if (in_span <= 0 || out_span <= 0) return 100;
  const int32_t scaled =
      kSocRuntimeKneeDisplayPct +
      ((raw_pct - kSocRuntimeKneeRawPct) * out_span + (in_span / 2)) / in_span;
  return clamp_i32(scaled, 0, 100);
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
  (void)state;
  (void)vbus_mv;
  (void)charge_state;
  // Keep the original behavior that worked on this hardware:
  // <= 50mA means mains/charging path, otherwise battery mode.
  return current_ma <= kCurrentMainsThresholdMa;
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
  if (have_display_level) {
    // Use one consistent scale for battery + mains to prevent visual jumps
    // when plugging/unplugging power.
    s.out.level_pct = map_soc_to_usable_percent(s.display_pct);
    // If charging above reserve floor, show at least 1%.
    if (s.out.on_mains && s.out.charging && s.display_pct > kSocReserveFloorPct) {
      if (s.out.level_pct < kSocChargingMinDisplayPct) {
        s.out.level_pct = kSocChargingMinDisplayPct;
      }
    }
  } else {
    s.out.level_pct = -1;
  }
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
