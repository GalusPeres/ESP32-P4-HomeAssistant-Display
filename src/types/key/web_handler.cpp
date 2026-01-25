#include "src/types/key/web_handler.h"
#include "src/game/key_parsing.h"

void apply_key_fields_from_request(WebServer& server, Tile& tile) {
  tile.key_macro = server.hasArg("key_macro") ? server.arg("key_macro") : "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;

  uint8_t modifier = 0;
  uint8_t key_code = 0;
  parseKeyMacro(tile.key_macro, key_code, modifier);
  tile.key_code = key_code;
  tile.key_modifier = modifier;
}
