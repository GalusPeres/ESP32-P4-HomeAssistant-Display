#pragma once

#include <lvgl.h>
#include "src/devices/device_select.h"

#if defined(DEVICE_LAYOUT_480X480)
LV_FONT_DECLARE(ui_font_12);
LV_FONT_DECLARE(ui_font_14);
#endif
LV_FONT_DECLARE(ui_font_16);
LV_FONT_DECLARE(ui_font_20);
LV_FONT_DECLARE(ui_font_20_semibold);
LV_FONT_DECLARE(ui_font_24);
LV_FONT_DECLARE(ui_font_cyrillic_14);
LV_FONT_DECLARE(ui_font_cyrillic_16);
LV_FONT_DECLARE(ui_font_cyrillic_20);
LV_FONT_DECLARE(ui_font_cyrillic_24);
LV_FONT_DECLARE(ui_symbols_20);
LV_FONT_DECLARE(ui_symbols_24);
LV_FONT_DECLARE(ui_font_28);
LV_FONT_DECLARE(ui_font_32);
LV_FONT_DECLARE(ui_font_40);
LV_FONT_DECLARE(ui_font_48);
LV_FONT_DECLARE(ui_font_56);
LV_FONT_DECLARE(ui_font_64);
LV_FONT_DECLARE(ui_font_72);
LV_FONT_DECLARE(ui_font_80);
LV_FONT_DECLARE(ui_font_96);

// Gemeinsamer Zugriff fuer Uhr, Wochentage und kuenftige UI-Texte. Alle
// deklarierten Groessen enthalten denselben vollstaendigen UI-Zeichensatz.
static inline const lv_font_t* ui_font_for_size(uint8_t size) {
  switch (size) {
#if defined(DEVICE_LAYOUT_480X480)
    case 12: return &ui_font_12;
    case 14: return &ui_font_14;
#endif
    case 16: return &ui_font_16;
    case 20: return &ui_font_20;
    case 24: return &ui_font_24;
    case 28: return &ui_font_28;
    case 32: return &ui_font_32;
    case 40: return &ui_font_40;
    case 48: return &ui_font_48;
    case 56: return &ui_font_56;
    case 64: return &ui_font_64;
    case 72: return &ui_font_72;
    case 80: return &ui_font_80;
    case 96: return &ui_font_96;
    default: return &ui_font_20;
  }
}
