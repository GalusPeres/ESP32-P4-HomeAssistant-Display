#pragma once

#include "src/tiles/tile_renderer.h"

struct ClockWidgetConfig {
  bool show_time = true;
  bool show_date = false;
  // Wochentag als Praefix der Datumszeile ("Dienstag, 15.07.2026"); ohne
  // Datum steht der Wochentag allein in der Zeile.
  bool show_weekday = false;
  // Sprachcode fuer den Wochentagsnamen (nullptr = Englisch-Fallback)
  const char* weekday_language = nullptr;
  bool fill_parent = false;
  // LVGL kennt keinen Glyphen-Schatten: hinter jede Zeile werden mehrere
  // leicht versetzte dunkle Duplikat-Labels gelegt (Fake-Blur). Wird nur vom
  // Screensaver genutzt.
  bool text_shadow = false;
  uint8_t time_font_size = 40;
  uint8_t date_font_size = 20;
  uint8_t time_format = 0;
  uint8_t date_format = 0;
  uint8_t time_alignment = 1;  // 0=left, 1=center, 2=right
  uint8_t date_alignment = 1;
};

// Gemeinsamer, rahmenloser Uhr-Inhalt fuer Clock-Tile und Screensaver. Das
// Objekt besitzt seinen eigenen Sekundentimer und raeumt ihn beim Loeschen auf.
lv_obj_t* create_clock_widget(lv_obj_t* parent, const ClockWidgetConfig& config);

lv_obj_t* render_clock_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
