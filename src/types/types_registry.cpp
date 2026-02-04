#include "src/types/types_registry.h"

#include <cstdio>
#include <cstring>
#include <inttypes.h>

#include "src/types/clock/renderer.h"
#include "src/types/empty/renderer.h"
#include "src/types/image/renderer.h"
#include "src/types/key/renderer.h"
#include "src/types/navigate/renderer.h"
#include "src/types/scene/renderer.h"
#include "src/types/sensor/renderer.h"
#include "src/types/switch/renderer.h"
#include "src/types/text/renderer.h"
#include "src/types/counter/renderer.h"
#include "src/types/weather/renderer.h"

#include "src/types/clock/web_handler.h"
#include "src/types/image/web_handler.h"
#include "src/types/key/web_handler.h"
#include "src/types/navigate/web_handler.h"
#include "src/types/scene/web_handler.h"
#include "src/types/sensor/web_handler.h"
#include "src/types/switch/web_handler.h"
#include "src/types/text/web_handler.h"
#include "src/types/counter/web_handler.h"
#include "src/types/weather/web_handler.h"

#include "src/types/clock/web_html.h"
#include "src/types/image/web_html.h"
#include "src/types/key/web_html.h"
#include "src/types/navigate/web_html.h"
#include "src/types/scene/web_html.h"
#include "src/types/sensor/web_html.h"
#include "src/types/switch/web_html.h"
#include "src/types/text/web_html.h"
#include "src/types/counter/web_html.h"
#include "src/types/weather/web_html.h"

#include "src/types/clock/web_scripts.h"
#include "src/types/image/web_scripts.h"
#include "src/types/key/web_scripts.h"
#include "src/types/navigate/web_scripts.h"
#include "src/types/scene/web_scripts.h"
#include "src/types/sensor/web_scripts.h"
#include "src/types/switch/web_scripts.h"
#include "src/types/text/web_scripts.h"
#include "src/types/counter/web_scripts.h"
#include "src/types/weather/web_scripts.h"

#include "src/types/clock/web_styles.h"
#include "src/types/image/web_styles.h"
#include "src/types/key/web_styles.h"
#include "src/types/navigate/web_styles.h"
#include "src/types/scene/web_styles.h"
#include "src/types/sensor/web_styles.h"
#include "src/types/switch/web_styles.h"
#include "src/types/text/web_styles.h"
#include "src/types/counter/web_styles.h"
#include "src/types/weather/web_styles.h"

#include "src/web/web_admin_utils.h"

namespace {

const String kEmptyString;
const std::vector<String> kEmptyStrings;
const std::vector<SceneOption> kEmptyScenes;

const String& safeString(const String* value) {
  return value ? *value : kEmptyString;
}

const std::vector<String>& safeStrings(const std::vector<String>* value) {
  return value ? *value : kEmptyStrings;
}

const std::vector<SceneOption>& safeScenes(const std::vector<SceneOption>* value) {
  return value ? *value : kEmptyScenes;
}

lv_obj_t* render_sensor_wrapper(lv_obj_t* parent,
                                int col,
                                int row,
                                const Tile& tile,
                                uint8_t index,
                                GridType grid_type,
                                scene_publish_cb_t) {
  return render_sensor_tile(parent, col, row, tile, index, grid_type);
}

lv_obj_t* render_scene_wrapper(lv_obj_t* parent,
                               int col,
                               int row,
                               const Tile& tile,
                               uint8_t index,
                               GridType,
                               scene_publish_cb_t scene_cb) {
  return render_scene_tile(parent, col, row, tile, index, scene_cb);
}

lv_obj_t* render_key_wrapper(lv_obj_t* parent,
                             int col,
                             int row,
                             const Tile& tile,
                             uint8_t index,
                             GridType grid_type,
                             scene_publish_cb_t) {
  return render_key_tile(parent, col, row, tile, index, grid_type);
}

lv_obj_t* render_navigate_wrapper(lv_obj_t* parent,
                                  int col,
                                  int row,
                                  const Tile& tile,
                                  uint8_t index,
                                  GridType,
                                  scene_publish_cb_t) {
  return render_navigate_tile(parent, col, row, tile, index);
}

lv_obj_t* render_switch_wrapper(lv_obj_t* parent,
                                int col,
                                int row,
                                const Tile& tile,
                                uint8_t index,
                                GridType grid_type,
                                scene_publish_cb_t) {
  return render_switch_tile(parent, col, row, tile, index, grid_type);
}

lv_obj_t* render_image_wrapper(lv_obj_t* parent,
                               int col,
                               int row,
                               const Tile& tile,
                               uint8_t index,
                               GridType,
                               scene_publish_cb_t) {
  return render_image_tile(parent, col, row, tile, index);
}

lv_obj_t* render_clock_wrapper(lv_obj_t* parent,
                               int col,
                               int row,
                               const Tile& tile,
                               uint8_t index,
                               GridType,
                               scene_publish_cb_t) {
  return render_clock_tile(parent, col, row, tile, index);
}

lv_obj_t* render_text_wrapper(lv_obj_t* parent,
                              int col,
                              int row,
                              const Tile& tile,
                              uint8_t index,
                              GridType,
                              scene_publish_cb_t) {
  return render_text_tile(parent, col, row, tile, index);
}

lv_obj_t* render_counter_wrapper(lv_obj_t* parent,
                                 int col,
                                 int row,
                                 const Tile& tile,
                                 uint8_t index,
                                 GridType grid_type,
                                 scene_publish_cb_t) {
  return render_counter_tile(parent, col, row, tile, index, grid_type);
}

lv_obj_t* render_weather_wrapper(lv_obj_t* parent,
                                 int col,
                                 int row,
                                 const Tile& tile,
                                 uint8_t index,
                                 GridType grid_type,
                                 scene_publish_cb_t) {
  return render_weather_tile(parent, col, row, tile, index, grid_type);
}

lv_obj_t* render_empty_wrapper(lv_obj_t* parent,
                               int col,
                               int row,
                               const Tile&,
                               uint8_t,
                               GridType,
                               scene_publish_cb_t) {
  return render_empty_tile(parent, col, row);
}

bool apply_sensor_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_sensor_fields_from_request(server, tile);
  return true;
}

bool apply_scene_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_scene_fields_from_request(server, tile);
  return true;
}

bool apply_key_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_key_fields_from_request(server, tile);
  return true;
}

bool apply_navigate_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext& ctx) {
  if (!ctx.tile_config || !ctx.error_message) {
    return false;
  }
  return apply_navigate_fields_from_request(server, tile, ctx.folder_id, *ctx.tile_config, *ctx.error_message);
}

bool apply_switch_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_switch_fields_from_request(server, tile);
  return true;
}

bool apply_image_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_image_fields_from_request(server, tile);
  return true;
}

bool apply_clock_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_clock_fields_from_request(server, tile);
  return true;
}

bool apply_text_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_text_fields_from_request(server, tile);
  return true;
}

bool apply_counter_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_counter_fields_from_request(server, tile);
  return true;
}

bool apply_weather_wrapper(WebServer& server, Tile& tile, const TileTypeApplyContext&) {
  apply_weather_fields_from_request(server, tile);
  return true;
}

bool apply_settings_wrapper(WebServer&, Tile& tile, const TileTypeApplyContext&) {
  if (!tile.title.length()) tile.title = "Settings";
  if (!tile.icon_name.length()) tile.icon_name = "cog";
  tile.sensor_decimals = 0xFF;
  tile.key_code = 0;
  tile.key_modifier = 0;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  return true;
}

bool apply_back_wrapper(WebServer&, Tile& tile, const TileTypeApplyContext&) {
  if (!tile.icon_name.length()) tile.icon_name = "arrow-left";
  tile.sensor_decimals = 0xFF;
  tile.key_code = 0;
  tile.key_modifier = 0;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  return true;
}

void append_sensor_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_sensor_fields_html(html, safeString(ctx.tab_id), safeStrings(ctx.sensor_options));
}

void append_scene_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_scene_fields_html(html, safeString(ctx.tab_id), safeScenes(ctx.scene_options));
}

void append_key_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_key_fields_html(html, safeString(ctx.tab_id));
}

void append_navigate_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_navigate_fields_html(html, safeString(ctx.tab_id), safeString(ctx.navigate_options_html));
}

void append_switch_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_switch_fields_html(html, safeString(ctx.tab_id), safeStrings(ctx.switch_options));
}

void append_image_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_image_fields_html(html, safeString(ctx.tab_id));
}

void append_clock_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_clock_fields_html(html, safeString(ctx.tab_id));
}

void append_text_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_text_fields_html(html, safeString(ctx.tab_id));
}

void append_counter_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_counter_fields_html(html, safeString(ctx.tab_id));
}

void append_weather_fields_wrapper(String& html, const TileTypeWebContext& ctx) {
  append_weather_fields_html(html, safeString(ctx.tab_id), safeStrings(ctx.weather_options));
}

const TileTypeDescriptor kTileTypes[] = {
  {
    TILE_EMPTY,
    "Leer",
    "empty",
    nullptr,
    "none",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    0,
    false,
    render_empty_wrapper,
    nullptr,
    nullptr,
    nullptr,
    nullptr
  },
  {
    TILE_SENSOR,
    "Sensor",
    "sensor",
    "sensor",
    "sensor",
    nullptr,
    "loadSensorFields",
    "saveSensorFields",
    "resetSensorFields",
    0x2A2A2A,
    false,
    render_sensor_wrapper,
    apply_sensor_wrapper,
    append_sensor_fields_wrapper,
    append_sensor_styles,
    append_sensor_scripts
  },
  {
    TILE_WEATHER,
    "Wetter",
    "weather",
    "weather",
    "weather",
    nullptr,
    "loadWeatherFields",
    "saveWeatherFields",
    "resetWeatherFields",
    0x2A2A2A,
    false,
    render_weather_wrapper,
    apply_weather_wrapper,
    append_weather_fields_wrapper,
    append_weather_styles,
    append_weather_scripts
  },
  {
    TILE_SCENE,
    "Szene",
    "scene",
    "scene",
    "none",
    nullptr,
    "loadSceneFields",
    "saveSceneFields",
    "resetSceneFields",
    0x353535,
    false,
    render_scene_wrapper,
    apply_scene_wrapper,
    append_scene_fields_wrapper,
    append_scene_styles,
    append_scene_scripts
  },
  {
    TILE_KEY,
    "Key",
    "key",
    "key",
    "none",
    nullptr,
    "loadKeyFields",
    "saveKeyFields",
    "resetKeyFields",
    0x353535,
    false,
    render_key_wrapper,
    apply_key_wrapper,
    append_key_fields_wrapper,
    append_key_styles,
    append_key_scripts
  },
  {
    TILE_FOLDER,
    "Ordner",
    "navigate",
    "navigate",
    "none",
    nullptr,
    "loadNavigateFields",
    "saveNavigateFields",
    "resetNavigateFields",
    0x353535,
    false,
    render_navigate_wrapper,
    apply_navigate_wrapper,
    append_navigate_fields_wrapper,
    append_navigate_styles,
    append_navigate_scripts
  },
  {
    TILE_SWITCH,
    "Schalter",
    "switch",
    "switch",
    "switch",
    nullptr,
    "loadSwitchFields",
    "saveSwitchFields",
    "resetSwitchFields",
    0x353535,
    false,
    render_switch_wrapper,
    apply_switch_wrapper,
    append_switch_fields_wrapper,
    append_switch_styles,
    append_switch_scripts
  },
  {
    TILE_IMAGE,
    "Bild",
    "image",
    "image",
    "none",
    "onImageTypeSelected",
    "loadImageFields",
    "saveImageFields",
    "resetImageFields",
    0x353535,
    false,
    render_image_wrapper,
    apply_image_wrapper,
    append_image_fields_wrapper,
    append_image_styles,
    append_image_scripts
  },
  {
    TILE_CLOCK,
    "Uhr",
    "clock",
    "clock",
    "clock",
    nullptr,
    "loadClockFields",
    "saveClockFields",
    "resetClockFields",
    0x353535,
    false,
    render_clock_wrapper,
    apply_clock_wrapper,
    append_clock_fields_wrapper,
    append_clock_styles,
    append_clock_scripts
  },
  {
    TILE_TEXT,
    "Text",
    "text",
    "text",
    "text",
    nullptr,
    "loadTextFields",
    "saveTextFields",
    "resetTextFields",
    0x353535,
    false,
    render_text_wrapper,
    apply_text_wrapper,
    append_text_fields_wrapper,
    append_text_styles,
    append_text_scripts
  },
  {
    TILE_COUNTER,
    "Counter",
    "counter",
    "counter",
    "counter",
    nullptr,
    "loadCounterFields",
    "saveCounterFields",
    "resetCounterFields",
    0x353535,
    false,
    render_counter_wrapper,
    apply_counter_wrapper,
    append_counter_fields_wrapper,
    append_counter_styles,
    append_counter_scripts
  },
  {
    TILE_SETTINGS,
    "Settings",
    "navigate",
    "navigate",
    "none",
    nullptr,
    nullptr,
    nullptr,
    "resetNavigateFields",
    0x353535,
    true,
    render_navigate_wrapper,
    apply_settings_wrapper,
    nullptr,
    nullptr,
    nullptr
  },
  {
    TILE_BACK,
    "Zurueck",
    "navigate",
    "navigate",
    "none",
    nullptr,
    nullptr,
    nullptr,
    "resetNavigateFields",
    0x353535,
    true,
    render_navigate_wrapper,
    apply_back_wrapper,
    nullptr,
    nullptr,
    nullptr
  }
};

const TileTypeDescriptor* find_descriptor(TileType type) {
  for (const auto& entry : kTileTypes) {
    if (entry.type == type) return &entry;
  }
  return nullptr;
}

}  // namespace

const TileTypeDescriptor* get_tile_type_descriptor(TileType type) {
  return find_descriptor(type);
}

uint32_t get_tile_type_default_bg(TileType type) {
  const TileTypeDescriptor* desc = find_descriptor(type);
  return desc ? desc->default_bg_color : 0;
}

const char* get_tile_type_css_class(TileType type) {
  const TileTypeDescriptor* desc = find_descriptor(type);
  return desc ? desc->css_class : nullptr;
}

const char* get_tile_type_preview_kind(TileType type) {
  const TileTypeDescriptor* desc = find_descriptor(type);
  return desc ? desc->preview_kind : nullptr;
}

void append_tile_type_fields_html(String& html, const TileTypeWebContext& ctx) {
  for (const auto& entry : kTileTypes) {
    if (entry.append_fields) {
      entry.append_fields(html, ctx);
    }
  }
}

void append_tile_type_styles(String& html) {
  for (const auto& entry : kTileTypes) {
    if (entry.append_styles) {
      entry.append_styles(html);
    }
  }
}

void append_tile_type_scripts(String& html) {
  for (const auto& entry : kTileTypes) {
    if (entry.append_scripts) {
      entry.append_scripts(html);
    }
  }
}

void append_tile_type_select_options(String& html) {
  for (const auto& entry : kTileTypes) {
    html += "<option value=\"";
    html += String(static_cast<unsigned>(entry.type));
    html += "\">";
    html += entry.label;
    html += "</option>";
  }
}

void append_tile_type_registry_js(String& html) {
  html += "  const TILE_TYPE_REGISTRY = {";
  for (const auto& entry : kTileTypes) {
    html += "\"";
    html += String(static_cast<unsigned>(entry.type));
    html += "\":{";
    if (entry.label && entry.label[0]) {
      html += "label:\"";
      html += entry.label;
      html += "\",";
    }
    if (entry.css_class && entry.css_class[0]) {
      html += "css:\"";
      html += entry.css_class;
      html += "\",";
    }
    if (entry.fields_suffix && entry.fields_suffix[0]) {
      html += "fields:\"";
      html += entry.fields_suffix;
      html += "\",";
    }
    if (entry.preview_kind && entry.preview_kind[0]) {
      html += "preview:\"";
      html += entry.preview_kind;
      html += "\",";
    }
    if (entry.js_on_select && entry.js_on_select[0]) {
      html += "onSelect:\"";
      html += entry.js_on_select;
      html += "\",";
    }
    if (entry.js_load && entry.js_load[0]) {
      html += "load:\"";
      html += entry.js_load;
      html += "\",";
    }
    if (entry.js_save && entry.js_save[0]) {
      html += "save:\"";
      html += entry.js_save;
      html += "\",";
    }
    if (entry.js_reset && entry.js_reset[0]) {
      html += "reset:\"";
      html += entry.js_reset;
      html += "\",";
    }
    if (entry.default_bg_color) {
      char color_hex[10] = {0};
      const uint32_t color24 = static_cast<uint32_t>(entry.default_bg_color) & 0x00FFFFFFu;
      snprintf(color_hex, sizeof(color_hex), "#%06" PRIX32, color24);
      html += "defaultBg:\"";
      html += color_hex;
      html += "\",";
    }
    html += "locked:";
    html += entry.locked ? "true" : "false";
    html += "},";
  }
  html += "};\n";
}
