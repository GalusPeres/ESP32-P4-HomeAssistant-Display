#include "src/ui/tab_home.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdint.h>
#include <lvgl.h>
#include "src/core/config_manager.h"

/* === Layout-Konstanten === */
static const int SENSOR_CARD_H = 175;
static const int SCENE_BTN_H = 130;
static const int GAP = 22;
static const int OUTER = 0;
static const int GRID_PAD = 22;

/* === Fonts === */
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  #define FONT_TITLE (&lv_font_montserrat_24)
#elif defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
  #define FONT_TITLE (&lv_font_montserrat_22)
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  #define FONT_TITLE (&lv_font_montserrat_20)
#else
  #define FONT_TITLE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  #define FONT_VALUE (&lv_font_montserrat_48)
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  #define FONT_VALUE (&lv_font_montserrat_40)
#elif defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
  #define FONT_VALUE (&lv_font_montserrat_36)
#else
  #define FONT_VALUE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
  #define FONT_UNIT (&lv_font_montserrat_28)
#elif defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  #define FONT_UNIT (&lv_font_montserrat_24)
#else
  #define FONT_UNIT (FONT_TITLE)
#endif

/* === Globale State === */
struct SensorTileWidgets {
  lv_obj_t* value_label = nullptr;
  lv_obj_t* unit_label = nullptr;
};

static lv_obj_t* g_home_grid = nullptr;
static SensorTileWidgets g_sensor_tiles[HA_SENSOR_SLOT_COUNT];
static String g_sensor_cache[HA_SENSOR_SLOT_COUNT];
static String g_sensor_entities[HA_SENSOR_SLOT_COUNT];
static String g_sensor_units[HA_SENSOR_SLOT_COUNT];
static String g_scene_aliases[HA_SCENE_SLOT_COUNT];
static scene_publish_cb_t g_scene_cb = nullptr;

/* === Helfer === */
static void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f) {
  lv_obj_set_style_text_color(lbl, c, 0);
  lv_obj_set_style_text_font(lbl,  f, 0);
}

static String make_display_label(const String& raw, bool strip_domain) {
  if (!raw.length()) return String("--");
  String text = raw;
  if (strip_domain) {
    int dot = text.indexOf('.');
    if (dot >= 0) {
      text = text.substring(dot + 1);
    }
  }
  text.replace('_', ' ');
  bool new_word = true;
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text.charAt(i);
    if (isalpha(static_cast<unsigned char>(c))) {
      char mapped = new_word ? toupper(static_cast<unsigned char>(c))
                             : tolower(static_cast<unsigned char>(c));
      text.setCharAt(i, mapped);
      new_word = false;
    } else {
      new_word = (c == ' ' || c == '-' || c == '/');
    }
  }
  text.trim();
  return text.length() ? text : raw;
}

static String sanitize_sensor_value(String raw) {
  raw.trim();
  raw.replace(",", ".");
  String lowered = raw;
  lowered.toLowerCase();
  if (!raw.length() || lowered == "unknown" || lowered == "unavailable" || lowered == "none" || lowered == "null") {
    return String("--");
  }
  return raw;
}

static bool unit_prefers_integer(const String& unit) {
  if (!unit.length()) return false;
  String lower = unit;
  lower.toLowerCase();
  return lower == "w" || lower == "kw" || lower == "a" || lower == "%";
}

static String format_sensor_value(uint8_t slot, const String& raw) {
  if (!raw.length() || raw == "--") {
    return raw;
  }
  if (slot >= HA_SENSOR_SLOT_COUNT) {
    return raw;
  }
  const String& unit = g_sensor_units[slot];
  if (!unit_prefers_integer(unit)) {
    return raw;
  }
  float val = raw.toFloat();
  long rounded = lroundf(val);
  return String(rounded);
}

static SensorTileWidgets make_sensor_card(lv_obj_t* parent, int col, int row,
                                          const char* title, const char* value,
                                          const char* unit) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_set_height(card, SENSOR_CARD_H);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(card,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* t = lv_label_create(card);
  set_label_style(t, lv_color_hex(0xFFFFFF), FONT_TITLE);
  lv_label_set_text(t, title);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  // Wert-Label direkt auf card (spart 1 Container-Objekt pro Kachel!)
  lv_obj_t* v = lv_label_create(card);
  set_label_style(v, lv_color_white(), FONT_VALUE);
  lv_label_set_text(v, value);
  lv_obj_align(v, LV_ALIGN_CENTER, -30, 18);

  // Einheit-Label direkt daneben
  lv_obj_t* u = lv_label_create(card);
  set_label_style(u, lv_color_hex(0xE6E6E6), FONT_UNIT);
  lv_label_set_text(u, unit ? unit : "");
  lv_obj_align_to(u, v, LV_ALIGN_OUT_RIGHT_MID, 14, 0);

  return {v, u};
}

static lv_obj_t* make_scene_button(lv_obj_t* parent, int col, int row,
                                   const char* text, uint8_t slot) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 18, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x353535), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x454545), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, SCENE_BTN_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* l = lv_label_create(btn);
  set_label_style(l, lv_color_white(), FONT_TITLE);
  lv_label_set_text(l, text);
  lv_obj_center(l);

  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        uintptr_t slot = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
        if (slot >= HA_SCENE_SLOT_COUNT) return;
        if (!g_scene_cb) return;
        const String& alias = g_scene_aliases[slot];
        if (!alias.length()) return;
        g_scene_cb(alias.c_str());
      },
      LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<uintptr_t>(slot)));

  return btn;
}

/* === Aufbau Home-Tab === */
void build_home_tab(lv_obj_t *parent, scene_publish_cb_t scene_cb) {
  g_scene_cb = scene_cb;

  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_anim_duration(parent, 0, 0);
  lv_obj_set_style_pad_left(parent,   OUTER, 0);
  lv_obj_set_style_pad_right(parent,  OUTER, 0);
  lv_obj_set_style_pad_top(parent,    OUTER, 0);
  lv_obj_set_style_pad_bottom(parent, OUTER, 0);

  g_home_grid = lv_obj_create(parent);
  lv_obj_set_style_bg_color(g_home_grid, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_home_grid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_home_grid, 0, 0);
  lv_obj_set_style_pad_left(g_home_grid, GRID_PAD, 0);
  lv_obj_set_style_pad_right(g_home_grid, GRID_PAD, 0);
  lv_obj_set_style_pad_top(g_home_grid, GRID_PAD, 0);
  lv_obj_set_style_pad_bottom(g_home_grid, GRID_PAD, 0);
  lv_obj_remove_flag(g_home_grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(g_home_grid, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_column(g_home_grid, GAP, 0);
  lv_obj_set_style_pad_row(g_home_grid, GAP, 0);

  static lv_coord_t col_dsc[] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t row_dsc[] = {
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_TEMPLATE_LAST
  };
  lv_obj_set_grid_dsc_array(g_home_grid, col_dsc, row_dsc);

  home_reload_layout();
}

void home_reload_layout() {
  if (!g_home_grid) return;

  Serial.printf("[Home] Heap vor reload: free=%u, min-free=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());

  lv_obj_clean(g_home_grid);
  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    g_sensor_tiles[i] = {};
    if (!g_sensor_entities[i].length()) {
      g_sensor_cache[i] = "--";
    }
  }

  // MQTT-Warnung entfernt - nervt beim Start
  // if (!configManager.hasMqttConfig()) {
  //   lv_obj_t* notice = lv_label_create(g_home_grid);
  //   lv_label_set_text(
  //       notice,
  //       LV_SYMBOL_WARNING " MQTT noch nicht konfiguriert\n"
  //       "Oeffne Einstellungen > WLAN/MQTT um Datenquellen festzulegen.");
  //   lv_obj_set_style_text_color(notice, lv_color_hex(0xFFC04D), 0);
  // #if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  //   lv_obj_set_style_text_font(notice, &lv_font_montserrat_24, 0);
  // #endif
  //   lv_obj_set_style_text_align(notice, LV_TEXT_ALIGN_CENTER, 0);
  //   lv_obj_set_width(notice, LV_PCT(100));
  //   lv_obj_set_grid_cell(notice,
  //                        LV_GRID_ALIGN_CENTER, 0, 3,
  //                        LV_GRID_ALIGN_CENTER, 0, 4);
  //   return;
  // }

  if (!configManager.hasMqttConfig()) {
    return;  // Einfach leeres Home-Tab anzeigen wenn MQTT nicht konfiguriert
  }

  // Prüfen ob HA-Daten verfügbar sind (nach MQTT-Discovery)
  if (!haBridgeConfig.hasData()) {
    Serial.println("[Home] Warte auf MQTT-Discovery, keine Kacheln gebaut");
    return;  // Keine Kacheln bauen, solange keine HA-Daten da sind
  }

  const HaBridgeConfigData& cfg = haBridgeConfig.get();

  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    uint8_t row = (i < 3) ? 0 : 1;
    uint8_t col = i % 3;
    const String& entity = cfg.sensor_slots[i];
    bool changed = (entity != g_sensor_entities[i]);
    g_sensor_entities[i] = entity;
    if (!entity.length()) {
      g_sensor_cache[i] = "";
      g_sensor_units[i] = "";
      continue;
    }
    if (changed) {
      g_sensor_cache[i] = "--";
    }
    if (changed || !g_sensor_cache[i].length() || g_sensor_cache[i] == "--") {
      String initial = haBridgeConfig.findSensorInitialValue(entity);
      if (initial.length()) {
        g_sensor_cache[i] = sanitize_sensor_value(initial);
      }
    }
    if (!g_sensor_cache[i].length()) {
      g_sensor_cache[i] = "--";
    }
    String title = cfg.sensor_titles[i];
    if (!title.length()) {
      title = haBridgeConfig.findSensorName(entity);
    }
    if (!title.length()) {
      title = make_display_label(entity, true);
    }
    const char* current = g_sensor_cache[i].c_str();
    String unit = cfg.sensor_custom_units[i];
    if (!unit.length()) {
      unit = haBridgeConfig.findSensorUnit(entity);
    }
    g_sensor_units[i] = unit;
    g_sensor_tiles[i] = make_sensor_card(g_home_grid, col, row, title.c_str(), current, unit.c_str());
  }

  Serial.printf("[Home] Heap nach Sensoren: free=%u, min-free=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());

  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    uint8_t row = 2 + (i / 3);
    uint8_t col = i % 3;
    const String& alias = cfg.scene_slots[i];
    g_scene_aliases[i] = alias;
    if (!alias.length()) {
      continue;
    }
    String title = cfg.scene_titles[i];
    if (!title.length()) {
      title = make_display_label(alias, false);
    }
    make_scene_button(g_home_grid, col, row, title.c_str(), static_cast<uint8_t>(i));
  }

  Serial.printf("[Home] Heap nach Szenen: free=%u, min-free=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void home_set_sensor_slot_value(uint8_t slot, const char* value) {
  if (slot >= HA_SENSOR_SLOT_COUNT) return;
  String sanitized = sanitize_sensor_value(value ? String(value) : String());
  sanitized = format_sensor_value(slot, sanitized);
  g_sensor_cache[slot] = sanitized;
  if (g_sensor_tiles[slot].value_label) {
    lv_label_set_text(g_sensor_tiles[slot].value_label, sanitized.c_str());
  }
}
