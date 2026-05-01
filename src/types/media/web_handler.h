#pragma once

#include <WebServer.h>

#include "src/tiles/tile_config.h"

void apply_media_fields_from_request(WebServer& server, Tile& tile);
