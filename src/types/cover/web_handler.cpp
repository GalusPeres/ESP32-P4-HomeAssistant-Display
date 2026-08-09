#include "src/types/cover/web_handler.h"

void apply_cover_fields_from_request(WebServer& server, Tile& tile) {
  tile.sensor_entity =
      server.hasArg("cover_entity") ? server.arg("cover_entity") : "";
  const uint8_t popup_mode =
      server.hasArg("popup_open_mode") &&
              server.arg("popup_open_mode").toInt() ==
                  TILE_POPUP_OPEN_LONG_PRESS
          ? TILE_POPUP_OPEN_LONG_PRESS
          : TILE_POPUP_OPEN_SHORT_PRESS;
  setTilePopupOpenMode(tile, popup_mode);
  tile.sensor_unit = "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  tile.key_code = 0;
  tile.key_modifier = 0;
}
