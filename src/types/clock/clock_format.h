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

}  // namespace clock_tile
