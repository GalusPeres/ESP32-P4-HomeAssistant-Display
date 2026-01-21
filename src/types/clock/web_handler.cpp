#include "src/types/clock/web_handler.h"

void apply_clock_fields_from_request(WebServer& server, Tile& tile) {
  (void)server;
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_gauge_enabled = false;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  tile.key_code = 0;
  tile.key_modifier = 0;
}
