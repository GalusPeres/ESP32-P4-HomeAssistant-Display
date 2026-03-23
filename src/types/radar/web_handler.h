#pragma once

#include <WebServer.h>
#include "src/tiles/tile_config.h"

bool apply_radar_fields_from_request(WebServer& server, Tile& tile, String& error_message);
