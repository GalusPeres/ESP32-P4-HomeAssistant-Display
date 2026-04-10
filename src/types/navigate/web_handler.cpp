#include "src/types/navigate/web_handler.h"
#include "src/web/web_admin_tile_helpers.h"

bool apply_navigate_fields_from_request(
    WebServer& server,
    Tile& tile,
    uint16_t folder_id,
    TileConfig& tileConfig,
    String& error_message) {
  int raw = server.hasArg("navigate_target") ? server.arg("navigate_target").toInt() : -1;
  uint16_t target_id = 0;
  if (raw > 0 && tileConfig.folderExists(static_cast<uint16_t>(raw))) {
    target_id = static_cast<uint16_t>(raw);
  }

  if (target_id == 0 || !tileConfig.folderExists(target_id)) {
    uint16_t new_id = 0;
    if (!tileConfig.createFolder(folder_id, tile.title, tile.icon_name, new_id)) {
      error_message = "Folder create failed";
      return false;
    }
    target_id = new_id;
  }
  tileConfig.updateFolder(target_id, tile.title, tile.icon_name);

  tile.sensor_decimals = 0xFF;
  setNavigateTargetId(tile, target_id);
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  return true;
}
