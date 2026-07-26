#include "src/types/camera/web_handler.h"

void apply_camera_fields_from_request(WebServer& server, Tile& tile) {
  tile.sensor_entity =
      server.hasArg("camera_entity") ? server.arg("camera_entity") : "";
  tile.sensor_unit = "";
  tile.scene_alias = "";
  tile.key_macro = "";
  tile.image_path = "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  tile.key_code = 0;
  tile.key_modifier = 0;
}
