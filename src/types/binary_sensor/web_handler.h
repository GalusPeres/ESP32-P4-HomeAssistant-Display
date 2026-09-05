#pragma once

#include <WebServer.h>

#include "src/tiles/config/tile_config.h"

void apply_binary_sensor_fields_from_request(WebServer& server, Tile& tile);
