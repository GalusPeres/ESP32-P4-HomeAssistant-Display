#include "src/types/switch/web_handler.h"

void apply_switch_fields_from_request(WebServer& server, Tile& tile) {
  tile.sensor_entity = server.hasArg("switch_entity") ? server.arg("switch_entity") : "";
  uint8_t style = 0;
  if (server.hasArg("switch_style")) {
    int raw = server.arg("switch_style").toInt();
    style = (raw == 1) ? 1 : 0;
  }
  tile.sensor_decimals = style;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}
