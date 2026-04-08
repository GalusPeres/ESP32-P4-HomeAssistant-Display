#pragma once

#include <stdint.h>

namespace clock_tile {

enum TimeFormat : uint8_t {
  TIME_FORMAT_AUTO = 0,
  TIME_FORMAT_24H = 1,
  TIME_FORMAT_12H = 2,
};

enum DateFormat : uint8_t {
  DATE_FORMAT_AUTO = 0,
  DATE_FORMAT_DMY = 1,
  DATE_FORMAT_MDY = 2,
  DATE_FORMAT_YMD = 3,
};

inline bool language_prefers_german_locale(const char* language_code) {
  return language_code &&
         (language_code[0] == 'd' || language_code[0] == 'D');
}

inline uint8_t normalize_time_format(int raw) {
  switch (raw) {
    case TIME_FORMAT_24H:
    case TIME_FORMAT_12H:
      return static_cast<uint8_t>(raw);
    default:
      return TIME_FORMAT_AUTO;
  }
}

inline uint8_t normalize_date_format(int raw) {
  switch (raw) {
    case DATE_FORMAT_DMY:
    case DATE_FORMAT_MDY:
    case DATE_FORMAT_YMD:
      return static_cast<uint8_t>(raw);
    default:
      return DATE_FORMAT_AUTO;
  }
}

inline uint8_t default_time_format_for_language(const char* language_code) {
  return language_prefers_german_locale(language_code) ? TIME_FORMAT_24H : TIME_FORMAT_12H;
}

inline uint8_t default_date_format_for_language(const char* language_code) {
  return language_prefers_german_locale(language_code) ? DATE_FORMAT_DMY : DATE_FORMAT_MDY;
}

inline uint8_t resolve_time_format(int preferred_raw, int global_raw, const char* language_code) {
  const uint8_t preferred = normalize_time_format(preferred_raw);
  if (preferred != TIME_FORMAT_AUTO) return preferred;
  const uint8_t global = normalize_time_format(global_raw);
  if (global != TIME_FORMAT_AUTO) return global;
  return default_time_format_for_language(language_code);
}

inline uint8_t resolve_date_format(int preferred_raw, int global_raw, const char* language_code) {
  const uint8_t preferred = normalize_date_format(preferred_raw);
  if (preferred != DATE_FORMAT_AUTO) return preferred;
  const uint8_t global = normalize_date_format(global_raw);
  if (global != DATE_FORMAT_AUTO) return global;
  return default_date_format_for_language(language_code);
}

}  // namespace clock_tile
