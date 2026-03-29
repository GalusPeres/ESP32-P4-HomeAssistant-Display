#include "src/types/scene/web_handler.h"

void apply_scene_fields_from_request(WebServer& server, Tile& tile) {
  tile.scene_alias = server.hasArg("scene_alias") ? server.arg("scene_alias") : "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}
