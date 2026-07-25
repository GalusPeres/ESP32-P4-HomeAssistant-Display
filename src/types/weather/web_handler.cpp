#include "src/types/weather/web_handler.h"

void apply_weather_fields_from_request(WebServer& server, Tile& tile) {
  tile.sensor_entity = server.hasArg("weather_entity") ? server.arg("weather_entity") : "";
  tile.sensor_unit = "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  uint8_t popup_mode = TILE_POPUP_OPEN_SHORT_PRESS;
  if (server.hasArg("popup_open_mode")) {
    popup_mode = (server.arg("popup_open_mode").toInt() == TILE_POPUP_OPEN_SHORT_PRESS)
                     ? TILE_POPUP_OPEN_SHORT_PRESS
                     : TILE_POPUP_OPEN_LONG_PRESS;
  }
  setTilePopupOpenMode(tile, popup_mode);
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}
