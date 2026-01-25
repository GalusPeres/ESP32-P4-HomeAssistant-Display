#include "src/types/clock/web_handler.h"

void apply_clock_fields_from_request(WebServer& server, Tile& tile) {
  bool show_time = true;
  bool show_date = false;
  if (server.hasArg("clock_show_time")) {
    show_time = (server.arg("clock_show_time").toInt() == 1);
  }
  if (server.hasArg("clock_show_date")) {
    show_date = (server.arg("clock_show_date").toInt() == 1);
  }
  uint8_t flags = 0;
  if (show_time) flags |= 1;
  if (show_date) flags |= 2;
  if (flags == 0) flags = 1;
  tile.sensor_decimals = flags;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  tile.key_code = 0;
  tile.key_modifier = 0;
}
