#include "src/types/radar/web_handler.h"

namespace {

struct RadarPresetDef {
  const char* key;
  const char* bbox;
};

const RadarPresetDef kRadarPresets[] = {
  {"bayern", "47.0,8.9,50.8,14.0"},
  {"germany", "46.0,5.5,55.5,15.8"},
  {"south", "46.5,7.0,50.8,14.5"},
  {"custom", ""}
};

String normalize_preset(String preset) {
  preset.trim();
  preset.toLowerCase();
  for (const auto& entry : kRadarPresets) {
    if (preset == entry.key) return preset;
  }
  return "bayern";
}

bool parse_bbox(const String& raw, float& min_lat, float& min_lon, float& max_lat, float& max_lon) {
  String text = raw;
  text.trim();
  if (!text.length()) return false;
  float values[4] = {0, 0, 0, 0};
  int start = 0;
  for (int i = 0; i < 4; ++i) {
    int comma = text.indexOf(',', start);
    String part = (comma >= 0) ? text.substring(start, comma) : text.substring(start);
    part.trim();
    if (!part.length()) return false;
    values[i] = part.toFloat();
    if (!part.equals(String(values[i], 6))) {
      String normalized = String(values[i], 6);
      normalized.trim();
      if (values[i] == 0.0f && part != "0" && part != "0.0" && part != "0.00") {
        return false;
      }
    }
    if (i < 3 && comma < 0) return false;
    start = comma + 1;
  }
  if (text.indexOf(',', start) >= 0) return false;
  min_lat = values[0];
  min_lon = values[1];
  max_lat = values[2];
  max_lon = values[3];
  if (min_lat >= max_lat || min_lon >= max_lon) return false;
  if (min_lat < -90.0f || max_lat > 90.0f) return false;
  if (min_lon < -180.0f || max_lon > 180.0f) return false;
  return true;
}

}  // namespace

bool apply_radar_fields_from_request(WebServer& server, Tile& tile, String& error_message) {
  String preset = normalize_preset(server.hasArg("radar_preset") ? server.arg("radar_preset") : "bayern");
  String bbox = server.hasArg("radar_bbox") ? server.arg("radar_bbox") : "";
  bbox.trim();

  if (preset == "custom") {
    float min_lat = 0.0f;
    float min_lon = 0.0f;
    float max_lat = 0.0f;
    float max_lon = 0.0f;
    if (!parse_bbox(bbox, min_lat, min_lon, max_lat, max_lon)) {
      error_message = "Custom BBOX ungueltig";
      return false;
    }
  } else {
    bbox = "";
  }

  int frame_count = server.hasArg("radar_frame_count") ? server.arg("radar_frame_count").toInt() : 6;
  if (frame_count < 2) frame_count = 2;
  if (frame_count > 8) frame_count = 8;

  int frame_delay_ms = server.hasArg("radar_frame_delay_ms") ? server.arg("radar_frame_delay_ms").toInt() : 450;
  if (frame_delay_ms < 150) frame_delay_ms = 150;
  if (frame_delay_ms > 3000) frame_delay_ms = 3000;

  int refresh_sec = server.hasArg("radar_refresh_sec") ? server.arg("radar_refresh_sec").toInt() : 300;
  if (refresh_sec < 60) refresh_sec = 60;
  if (refresh_sec > 3600) refresh_sec = 3600;

  uint8_t popup_mode = TILE_POPUP_OPEN_LONG_PRESS;
  if (server.hasArg("popup_open_mode")) {
    popup_mode = (server.arg("popup_open_mode").toInt() == TILE_POPUP_OPEN_SHORT_PRESS)
                     ? TILE_POPUP_OPEN_SHORT_PRESS
                     : TILE_POPUP_OPEN_LONG_PRESS;
  }

  tile.sensor_entity = preset;
  tile.sensor_unit = bbox;
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = frame_count;
  tile.sensor_gauge_max = frame_delay_ms;
  tile.sensor_gauge_arc = 100;
  tile.sensor_gauge_size = 350;
  tile.sensor_gauge_y_offset = 12;
  tile.sensor_value_y_offset = 0;
  tile.sensor_graph_height = 60;
  tile.scene_alias = "";
  tile.key_macro = "";
  tile.key_code = 0;
  tile.key_modifier = 0;
  tile.image_path = "";
  tile.image_slideshow_sec = static_cast<uint16_t>(refresh_sec);
  setTilePopupOpenMode(tile, popup_mode);
  return true;
}
