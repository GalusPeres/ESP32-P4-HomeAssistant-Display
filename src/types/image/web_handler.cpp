#include "src/types/image/web_handler.h"
#include <Arduino.h>

void apply_image_fields_from_request(WebServer& server, Tile& tile) {
  tile.image_path = server.hasArg("image_path") ? server.arg("image_path") : "";
  tile.image_path.trim();
  tile.key_macro = "";
  Serial.printf("[WebAdmin] IMAGE Tile - Empfangener Pfad: '%s'\n", tile.image_path.c_str());
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}
