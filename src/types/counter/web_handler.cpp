#include "src/types/counter/web_handler.h"

void apply_counter_fields_from_request(WebServer& server, Tile& tile) {
  tile.scene_alias = server.hasArg("counter_value") ? server.arg("counter_value") : "0";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}
