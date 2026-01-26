#include "src/types/counter/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include <Arduino.h>

namespace {

struct CounterState {
  lv_obj_t* label = nullptr;
  int32_t value = 0;
};

static CounterState s_states[3][TILES_PER_GRID];
static lv_timer_t* s_save_timer = nullptr;

void counter_persist_cb(lv_timer_t* timer) {
  (void)timer;
  s_save_timer = nullptr;
  uint16_t folder_id = tileConfig.getActiveFolderId();
  TileGridConfig& grid = tileConfig.getActiveGrid();
  tileConfig.saveFolderGrid(folder_id, grid);
}

void counter_schedule_save() {
  if (s_save_timer) {
    lv_timer_reset(s_save_timer);
  } else {
    s_save_timer = lv_timer_create(counter_persist_cb, 1000, nullptr);
    if (s_save_timer) lv_timer_set_repeat_count(s_save_timer, 1);
  }
}

void counter_update_grid(uint8_t tile_idx, int32_t new_value) {
  TileGridConfig& grid = tileConfig.getActiveGrid();
  if (tile_idx < TILES_PER_GRID) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)new_value);
    grid.tiles[tile_idx].scene_alias = buf;
  }
}

void counter_click_cb(lv_event_t* e) {
  uint32_t packed = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  uint8_t grid_idx = (packed >> 8) & 0xFF;
  uint8_t tile_idx = packed & 0xFF;
  if (grid_idx >= 3 || tile_idx >= TILES_PER_GRID) return;

  CounterState& st = s_states[grid_idx][tile_idx];
  st.value++;
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", (long)st.value);
  if (st.label) lv_label_set_text(st.label, buf);

  counter_update_grid(tile_idx, st.value);
  counter_schedule_save();
}

void counter_longpress_cb(lv_event_t* e) {
  uint32_t packed = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  uint8_t grid_idx = (packed >> 8) & 0xFF;
  uint8_t tile_idx = packed & 0xFF;
  if (grid_idx >= 3 || tile_idx >= TILES_PER_GRID) return;

  CounterState& st = s_states[grid_idx][tile_idx];
  st.value = 0;
  if (st.label) lv_label_set_text(st.label, "0");

  counter_update_grid(tile_idx, 0);
  counter_schedule_save();
}

}  // namespace

lv_obj_t* render_counter_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  lv_obj_t* card = lv_button_create(parent);
  if (!card) return nullptr;

  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  uint32_t pressed_color = card_color + 0x101010;
  lv_obj_set_style_bg_color(card, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 18, 0);
  lv_obj_set_style_pad_ver(card, 16, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  // Icon (optional)
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      lv_obj_t* icon_lbl = lv_label_create(card);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());
        lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, 4, -8);
      }
    }
  }

  // Title (optional)
  if (tile.title.length() > 0) {
    lv_obj_t* title_lbl = lv_label_create(card);
    if (title_lbl) {
      set_label_style(title_lbl, lv_color_hex(0xFFFFFF), FONT_TITLE);
      lv_label_set_text(title_lbl, tile.title.c_str());
      lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 4);
    }
  }

  // Parse initial value from scene_alias
  int32_t initial = 0;
  if (tile.scene_alias.length() > 0) {
    initial = tile.scene_alias.toInt();
  }

  uint8_t grid_idx = static_cast<uint8_t>(grid_type);
  if (grid_idx >= 3) grid_idx = 0;
  CounterState& st = s_states[grid_idx][index];
  st.value = initial;

  // Counter value label (centered)
  lv_obj_t* val_lbl = lv_label_create(card);
  if (val_lbl) {
    set_label_style(val_lbl, lv_color_white(), FONT_VALUE);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)initial);
    lv_label_set_text(val_lbl, buf);
    int16_t y_offset = (tile.title.length() || tile.icon_name.length()) ? 12 : 0;
    lv_obj_align(val_lbl, LV_ALIGN_CENTER, 0, y_offset);
  }
  st.label = val_lbl;

  // Events: tap = +1, long-press = reset to 0
  uint32_t packed = ((uint32_t)grid_idx << 8) | (uint32_t)index;
  void* ud = (void*)(uintptr_t)packed;
  lv_obj_add_event_cb(card, counter_click_cb, LV_EVENT_SHORT_CLICKED, ud);
  lv_obj_add_event_cb(card, counter_longpress_cb, LV_EVENT_LONG_PRESSED, ud);

  return card;
}
