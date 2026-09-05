#pragma once

#include "src/tiles/config/tile_config.h"

inline uint16_t getNavigateTargetId(const Tile& tile) {
  return static_cast<uint16_t>((static_cast<uint16_t>(tile.key_modifier) << 8) | tile.key_code);
}

inline void setNavigateTargetId(Tile& tile, uint16_t folder_id) {
  tile.key_code = static_cast<uint8_t>(folder_id & 0xFF);
  tile.key_modifier = static_cast<uint8_t>((folder_id >> 8) & 0xFF);
}
