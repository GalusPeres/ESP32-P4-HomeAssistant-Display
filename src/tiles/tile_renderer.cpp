#include "src/tiles/tile_renderer.h"
#include "src/network/ha_bridge_config.h"
#include "src/game/game_ws_server.h"
#include "src/tiles/tile_config.h"
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>

/* === Layout-Konstanten === */
static const int CARD_H = 150;

/* === Fonts === */
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  #define FONT_TITLE (&lv_font_montserrat_24)
#else
  #define FONT_TITLE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  #define FONT_VALUE (&lv_font_montserrat_48)
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  #define FONT_VALUE (&lv_font_montserrat_40)
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

/* === Globale State für Updates === */
struct SensorTileWidgets {
  lv_obj_t* value_label = nullptr;
  lv_obj_t* unit_label = nullptr;
};

static SensorTileWidgets g_tab0_sensors[TILES_PER_GRID];
static SensorTileWidgets g_tab1_sensors[TILES_PER_GRID];
static SensorTileWidgets g_tab2_sensors[TILES_PER_GRID];

static void clear_sensor_widgets(GridType grid_type) {
  SensorTileWidgets* target = g_tab0_sensors;
  if (grid_type == GridType::TAB1) target = g_tab1_sensors;
  else if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    target[i].value_label = nullptr;
    target[i].unit_label = nullptr;
  }
}

void reset_sensor_widget(GridType grid_type, uint8_t grid_index) {
  if (grid_index >= TILES_PER_GRID) return;
  SensorTileWidgets* target = g_tab0_sensors;
  if (grid_type == GridType::TAB1) target = g_tab1_sensors;
  else if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  target[grid_index] = {};
}

void reset_sensor_widgets(GridType grid_type) {
  clear_sensor_widgets(grid_type);
}

/* === Thread-Safe Update Queue (MQTT → Main Loop) === */
struct SensorUpdate {
  GridType grid_type;
  uint8_t grid_index;
  String value;
  String unit;
  bool valid;
};

static const uint8_t QUEUE_SIZE = 32;
static SensorUpdate g_update_queue[QUEUE_SIZE];
static volatile uint8_t g_queue_head = 0;
static volatile uint8_t g_queue_tail = 0;
static uint32_t g_queue_overflow_count = 0;

static uint8_t get_sensor_decimals(GridType grid_type, uint8_t grid_index) {
  if (grid_index >= TILES_PER_GRID) return 0xFF;
  const TileGridConfig& grid = (grid_type == GridType::TAB1)
                                 ? tileConfig.getGameGrid()
                                 : (grid_type == GridType::TAB2 ? tileConfig.getWeatherGrid() : tileConfig.getHomeGrid());
  return grid.tiles[grid_index].sensor_decimals;
}

static bool apply_decimals(String& value, uint8_t decimals) {
  if (decimals == 0xFF) return false;  // Keine Rundung gewuenscht
  String normalized = value;
  normalized.replace(",", ".");
  char* end = nullptr;
  float f = strtof(normalized.c_str(), &end);
  if (!end || end == normalized.c_str()) return false;  // Keine Zahl
  if (isnan(f) || isinf(f)) return false;
  uint8_t d = decimals > 6 ? 6 : decimals;
  value = String(f, static_cast<unsigned int>(d));
  return true;
}

// MQTT Callback ruft das auf (thread-safe!)
void queue_sensor_tile_update(GridType grid_type, uint8_t grid_index, const char* value, const char* unit) {
  if (grid_index >= TILES_PER_GRID || !value) {
    return;
  }

  // Bestehendes, noch nicht verarbeitetes Update fuer dieselbe Tile ersetzen
  uint8_t idx = g_queue_tail;
  while (idx != g_queue_head) {
    SensorUpdate& pending = g_update_queue[idx];
    if (pending.valid &&
        pending.grid_type == grid_type &&
        pending.grid_index == grid_index) {
      pending.value = String(value);
      pending.unit = unit ? String(unit) : "";
      return;
    }
    idx = (idx + 1) % QUEUE_SIZE;
  }

  uint8_t next_head = (g_queue_head + 1) % QUEUE_SIZE;

  if (next_head == g_queue_tail) {
    // Queue voll - aeltestes Element verwerfen und ueberschreiben
    if ((g_queue_overflow_count++ % 10) == 0) {
      Serial.println("[Queue] VOLL! Aeltestes Sensor-Update wird ueberschrieben");
    }
    g_queue_tail = (g_queue_tail + 1) % QUEUE_SIZE;
  }

  g_update_queue[g_queue_head].grid_type = grid_type;
  g_update_queue[g_queue_head].grid_index = grid_index;
  g_update_queue[g_queue_head].value = String(value);
  g_update_queue[g_queue_head].unit = unit ? String(unit) : "";
  g_update_queue[g_queue_head].valid = true;

  g_queue_head = next_head;
}

// Main Loop ruft das VOR lv_timer_handler() auf!
void process_sensor_update_queue() {
  while (g_queue_tail != g_queue_head) {
    SensorUpdate& upd = g_update_queue[g_queue_tail];

    if (upd.valid) {
      update_sensor_tile_value(upd.grid_type, upd.grid_index, upd.value.c_str(),
                              upd.unit.length() > 0 ? upd.unit.c_str() : nullptr);
      upd.valid = false;
    }

    g_queue_tail = (g_queue_tail + 1) % QUEUE_SIZE;
  }
}

/* === Helfer === */
static void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f) {
  lv_obj_set_style_text_color(lbl, c, 0);
  lv_obj_set_style_text_font(lbl, f, 0);
}

void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type, scene_publish_cb_t scene_cb) {
  // Memory Monitoring - Vorher
  uint32_t heap_before = ESP.getFreeHeap();
  uint32_t psram_before = ESP.getFreePsram();
  Serial.printf("[TileRenderer] Lade %d Tiles... | Heap: %u KB | PSRAM: %u KB\n",
                TILES_PER_GRID, heap_before / 1024, psram_before / 1024);

  // Reset sensor widget pointers for this grid to avoid stale references
  clear_sensor_widgets(grid_type);

  for (int i = 0; i < TILES_PER_GRID; ++i) {
    int row = i / 3;
    int col = i % 3;

    // Fehlerbehandlung: Ein defektes Tile crasht nicht das ganze System
    if (parent == nullptr) {
      Serial.println("[TileRenderer] ERROR: Parent ist NULL!");
      return;
    }

    Serial.printf("[TileRenderer] Erstelle Tile %d/%d...\n", i + 1, TILES_PER_GRID);

    render_tile(parent, col, row, config.tiles[i], i, grid_type, scene_cb);

    // PROGRESSIVES RENDERING: Pause zwischen Tiles (verhindert Crash)
    yield();                    // Watchdog füttern
    delay(10);                 // 10ms Pause für System Processing (120ms total)
    yield();                   // Nochmal Watchdog
    // KEIN lv_timer_handler() hier! Sonst werden unfertige Tiles gezeichnet!

    Serial.printf("[TileRenderer] ✓ Tile %d/%d fertig\n", i + 1, TILES_PER_GRID);
  }

  // Memory Monitoring - Nachher
  uint32_t heap_after = ESP.getFreeHeap();
  uint32_t psram_after = ESP.getFreePsram();
  int32_t heap_used = heap_before - heap_after;
  int32_t psram_used = psram_before - psram_after;

  Serial.printf("[TileRenderer] ✓ Alle Tiles geladen | Heap: %u KB (-%d KB) | PSRAM: %u KB (-%d KB)\n",
                heap_after / 1024, heap_used / 1024,
                psram_after / 1024, psram_used / 1024);
  Serial.printf("[TileRenderer] Min Free Heap seit Boot: %u KB\n", ESP.getMinFreeHeap() / 1024);
}

lv_obj_t* render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb) {
  switch (tile.type) {
    case TILE_SENSOR:
      return render_sensor_tile(parent, col, row, tile, index, grid_type);
    case TILE_SCENE:
      return render_scene_tile(parent, col, row, tile, index, scene_cb);
    case TILE_KEY:
      return render_key_tile(parent, col, row, tile, index, grid_type);
    default:
      return render_empty_tile(parent, col, row);
  }
  return nullptr;
}

lv_obj_t* render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  if (!parent) {
    Serial.println("[TileRenderer] ERROR: parent NULL bei Sensor-Tile");
    return nullptr;
  }

  lv_obj_t* card = lv_obj_create(parent);
  if (!card) {
    Serial.println("[TileRenderer] ERROR: Konnte Sensor-Card nicht erstellen");
    return nullptr;
  }

  // Farbe verwenden (Standard: 0x2A2A2A wenn color = 0)
  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x2A2A2A;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_set_height(card, CARD_H);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(card,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Title Label
  lv_obj_t* t = lv_label_create(card);
  if (!t) {
    Serial.println("[TileRenderer] ERROR: Konnte Title-Label nicht erstellen");
    return card;
  }
  set_label_style(t, lv_color_hex(0xFFFFFF), FONT_TITLE);
  lv_label_set_text(t, tile.title.length() ? tile.title.c_str() : "Sensor");
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  // Value Label (Wert + Einheit kombiniert)
  lv_obj_t* v = lv_label_create(card);
  if (!v) {
    Serial.println("[TileRenderer] ERROR: Konnte Value-Label nicht erstellen");
    return card;
  }
  set_label_style(v, lv_color_white(), FONT_VALUE);
  lv_label_set_text(v, "--");
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 18);

  // Speichern für spätere Updates
  SensorTileWidgets* target = (grid_type == GridType::TAB0) ? g_tab0_sensors : g_tab1_sensors;
  if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  target[index].value_label = v;
  target[index].unit_label = nullptr;

  return card;
}

struct SceneEventData {
  String scene_alias;
  scene_publish_cb_t callback;
};

lv_obj_t* render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, CARD_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* l = lv_label_create(btn);
  set_label_style(l, lv_color_white(), FONT_TITLE);
  lv_label_set_text(l, tile.title.length() ? tile.title.c_str() : "Szene");
  lv_obj_center(l);

  // Event-Handler für Scene-Aktivierung
  if (scene_cb && tile.scene_alias.length()) {
    // Allocate permanent storage for event data
    SceneEventData* event_data = new SceneEventData{tile.scene_alias, scene_cb};

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          SceneEventData* data = static_cast<SceneEventData*>(lv_event_get_user_data(e));
          if (data && data->callback) {
            Serial.printf("[Tile] Szene aktiviert: %s\n", data->scene_alias.c_str());
            data->callback(data->scene_alias.c_str());
          }
        },
        LV_EVENT_CLICKED,
        event_data);
  }

  return btn;
}

struct KeyEventData {
  String title;
  uint8_t key_code;
  uint8_t modifier;
  uint8_t index;
};

lv_obj_t* render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, CARD_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* l = lv_label_create(btn);
  set_label_style(l, lv_color_white(), FONT_TITLE);
  lv_label_set_text(l, tile.title.length() ? tile.title.c_str() : "Key");
  lv_obj_center(l);

  // Event-Handler für WebSocket Broadcast
  if (tile.key_code != 0) {
    // Allocate permanent storage for event data
    KeyEventData* event_data = new KeyEventData{
      tile.title,
      tile.key_code,
      tile.key_modifier,
      index
    };

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          KeyEventData* data = static_cast<KeyEventData*>(lv_event_get_user_data(e));
          if (data) {
            Serial.printf("[Tile] Key '%s' gedrückt - Code: 0x%02X Mod: 0x%02X\n",
                          data->title.c_str(), data->key_code, data->modifier);

            // WebSocket Broadcast an alle verbundenen Clients
            gameWSServer.broadcastButtonPress(
              data->index,
              data->title.c_str(),
              data->key_code,
              data->modifier
            );
          }
        },
        LV_EVENT_CLICKED,
        event_data);
  }

  return btn;
}

lv_obj_t* render_empty_tile(lv_obj_t* parent, int col, int row) {
  lv_obj_t* placeholder = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(placeholder, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(placeholder, 0, 0);
  lv_obj_set_height(placeholder, CARD_H);
  lv_obj_set_grid_cell(placeholder,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);
  return placeholder;
}

void update_sensor_tile_value(GridType grid_type, uint8_t grid_index, const char* value, const char* unit) {
  if (grid_index >= TILES_PER_GRID) {
    return;
  }

  SensorTileWidgets* target = (grid_type == GridType::TAB1) ? g_tab1_sensors : g_tab0_sensors;
  if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  lv_obj_t* value_label = target[grid_index].value_label;
  if (!value_label) {
    return;
  }

  String displayValue = value ? String(value) : String();
  displayValue.trim();

  String lower = displayValue;
  lower.toLowerCase();
  if (lower == "unavailable" || lower == "unknown" || lower == "none" || lower == "null") {
    displayValue = "--";
  }

  // Formatierung nach Wunsch der Kachel (Nachkommastellen)
  if (displayValue.length() > 0 &&
      displayValue != "--" &&
      !displayValue.equalsIgnoreCase("unavailable")) {
    uint8_t decimals = get_sensor_decimals(grid_type, grid_index);
    apply_decimals(displayValue, decimals);
  }

  // Zeige "--" wenn leer oder unavailable
  if (displayValue.length() == 0 || displayValue.equalsIgnoreCase("unavailable")) {
    displayValue = "--";
  }

  // Kombiniere Wert + Einheit in einem Label (gleiche Größe)
  String combined = displayValue;
  if (unit && strlen(unit) > 0 && displayValue != "--") {
    combined += " ";
    combined += unit;
  }
  lv_label_set_text(value_label, combined.c_str());
}
