#pragma once

#include <WebServer.h>
#include "src/tiles/tile_config.h"

void apply_counter_fields_from_request(WebServer& server, Tile& tile);
