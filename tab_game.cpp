#include "tab_game.h"
#include "game_controls_config.h"
#include "game_ws_server.h"
#include <Arduino.h>

/* === Layout-Konstanten === */
static const int BUTTON_H = 150;
static const int GAP = 22;
static const int OUTER = 22;
static const int GRID_PAD = 22;

/* === Fonts === */
#if defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
  #define FONT_BUTTON (&lv_font_montserrat_28)
#elif defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  #define FONT_BUTTON (&lv_font_montserrat_24)
#else
  #define FONT_BUTTON (LV_FONT_DEFAULT)
#endif

/* === Globale State === */
static lv_obj_t* g_game_grid = nullptr;

/* === Helfer === */
static lv_obj_t* make_game_button(lv_obj_t* parent, int col, int row,
                                   const char* text, uint8_t slot) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 18, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E4A6F), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, BUTTON_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* l = lv_label_create(btn);
  lv_obj_set_style_text_color(l, lv_color_white(), 0);
  lv_obj_set_style_text_font(l, FONT_BUTTON, 0);
  lv_label_set_text(l, text);
  lv_obj_center(l);

  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        uintptr_t slot = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
        if (slot >= GAME_BUTTON_COUNT) return;

        const GameControlsConfigData& cfg = gameControlsConfig.get();
        const GameButton& button = cfg.buttons[slot];

        if (!button.key_code) {
          Serial.printf("[Game] Button %u: Keine Taste konfiguriert\n", static_cast<unsigned>(slot));
          return;
        }

        Serial.printf("[Game] Button %u '%s' gedrueckt - Key: 0x%02X Mod: 0x%02X\n",
                      static_cast<unsigned>(slot),
                      button.name.c_str(),
                      button.key_code,
                      button.modifier);

        // WebSocket Broadcast an alle verbundenen Clients
        gameWSServer.broadcastButtonPress(
          static_cast<uint8_t>(slot),
          button.name.c_str(),
          button.key_code,
          button.modifier
        );
      },
      LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<uintptr_t>(slot)));

  return btn;
}

/* === Aufbau Game-Tab === */
void build_game_tab(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_anim_duration(parent, 0, 0);
  lv_obj_set_style_pad_all(parent, OUTER, 0);

  g_game_grid = lv_obj_create(parent);
  lv_obj_set_style_bg_color(g_game_grid, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_game_grid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_game_grid, 0, 0);
  lv_obj_set_style_pad_all(g_game_grid, GRID_PAD, 0);
  lv_obj_remove_flag(g_game_grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(g_game_grid, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_column(g_game_grid, GAP, 0);
  lv_obj_set_style_pad_row(g_game_grid, GAP, 0);

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
  lv_obj_set_grid_dsc_array(g_game_grid, col_dsc, row_dsc);

  game_reload_layout();
}

void game_reload_layout() {
  if (!g_game_grid) return;

  lv_obj_clean(g_game_grid);

  const GameControlsConfigData& cfg = gameControlsConfig.get();

  // 12 Buttons in 4 Reihen zu je 3 Spalten
  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    uint8_t row = i / 3;
    uint8_t col = i % 3;

    const GameButton& button = cfg.buttons[i];

    // Wenn keine Taste konfiguriert, leeren Button zeigen
    String label = button.name.length() ? button.name : String("---");

    make_game_button(g_game_grid, col, row, label.c_str(), static_cast<uint8_t>(i));
  }

  Serial.println("[Game] Layout neu geladen");
}
