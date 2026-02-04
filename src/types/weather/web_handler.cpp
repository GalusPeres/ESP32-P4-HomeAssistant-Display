#include "src/types/weather/web_handler.h"

void apply_weather_fields_from_request(WebServer& server, Tile& tile) {
  tile.sensor_entity = server.hasArg("weather_entity") ? server.arg("weather_entity") : "";
  tile.sensor_unit = "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}
