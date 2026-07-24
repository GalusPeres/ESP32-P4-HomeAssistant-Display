#include "src/ui/ui_keyboard.h"

#include <cstring>

#include "src/core/config_manager.h"
#include "src/fonts/ui_fonts.h"
#include "src/ui/popup_layout.h"

namespace {

// Deutsche Buchstaben-Layouts: LVGLs eingebaute Standardbelegung (siehe
// lv_keyboard.c) plus oe/ae/ue/ss, weil deutsche SSIDs/Passwoerter/Hostnamen
// sie brauchen koennen. Struktur (Zeilen, Tastengewichte, Sondertasten-Flags)
// 1:1 vom LVGL-Original uebernommen, nur die drei Buchstabenreihen erweitert.
// LV_BUTTONMATRIX_CTRL_POPOVER zeigt die Taste beim Druecken vergroessert an -
// wirkt aber erst zusammen mit lv_keyboard_set_popovers(kb, true) (siehe
// ui_keyboard_create): lv_keyboard_update_ctrl_map() entfernt das Flag sonst
// wieder aus jeder per lv_keyboard_set_map() gesetzten Control-Map.
// LV_BUTTONMATRIX_CTRL_CHECKED = dauerhaft dunklerer "Sondertasten"-Look
// (siehe kb_draw_task_cb). Muessen als static ueberleben, da die Buttonmatrix
// nur den Pointer haelt statt den Text zu kopieren.
// LVGLs Flag-Enum hat keinen festen Underlying-Type; "enum | int" verengt in
// C++ (anders als im C-Original lv_keyboard.c) implizit von int zurueck auf
// das Enum, was ohne expliziten Cast ein harter Fehler ist.
constexpr lv_buttonmatrix_ctrl_t kCtrl(int v) {
  return static_cast<lv_buttonmatrix_ctrl_t>(v);
}
constexpr lv_buttonmatrix_ctrl_t kBtn(uint8_t width) {
  return kCtrl(LV_BUTTONMATRIX_CTRL_POPOVER | width);
}

// "1#"/"ABC"/"abc" sind die Mode-Umschalter-Beschriftungen von lv_keyboard.c -
// dort nur privat als Makro definiert, deshalb hier als Literal uebernommen.
static const char* const kMapLowerDe[] = {
    "1#", "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "\xC3\xBC", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l", "\xC3\xB6", "\xC3\xA4", LV_SYMBOL_NEW_LINE, "\n",
    "_", "-", "y", "x", "c", "v", "b", "n", "m", "\xC3\x9F", ".", ",", ":", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""};

static const lv_buttonmatrix_ctrl_t kCtrlDe[] = {
    kCtrl(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 5), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kBtn(4), kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | 7),
    kCtrl(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kBtn(3), kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | 7),
    kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | kBtn(1)), kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | kBtn(1)), kBtn(1), kBtn(1), kBtn(1), kBtn(1), kBtn(1), kBtn(1), kBtn(1), kBtn(1), kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | kBtn(1)), kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | kBtn(1)), kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | kBtn(1)),
    kCtrl(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2), kCtrl(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2), kCtrl(6), kCtrl(LV_BUTTONMATRIX_CTRL_CHECKED | 2), kCtrl(LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2)};

static const char* const kMapUpperDe[] = {
    "1#", "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "\xC3\x9C", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L", "\xC3\x96", "\xC3\x84", LV_SYMBOL_NEW_LINE, "\n",
    "_", "-", "Y", "X", "C", "V", "B", "N", "M", "\xC3\x9F", ".", ",", ":", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""};

// Layout-Auswahl: explizite Einstellung (Lokalisierung -> Tastatur) gewinnt,
// "Auto" folgt der UI-Sprache. Nur Layouts mit abweichender Belegung (aktuell
// Deutsch/QWERTZ, wegen oe/ae/ue/ss) brauchen einen Eintrag - ohne Treffer
// bleibt LVGLs eingebaute Standard-Map (Englisch/QWERTY) aktiv.
struct KeyboardLayout {
  const char* const* lower_map;
  const char* const* upper_map;
  const lv_buttonmatrix_ctrl_t* ctrl_map;
};

const KeyboardLayout* layout_for_config(uint8_t keyboard_layout, const char* lang_code) {
  bool german;
  if (keyboard_layout == 1) {
    german = true;   // Deutsch (QWERTZ) erzwungen
  } else if (keyboard_layout == 2) {
    german = false;  // English (QWERTY) erzwungen
  } else {
    german = lang_code && lang_code[0] == 'd' && lang_code[1] == 'e';
  }
  if (german) {
    static const KeyboardLayout kDeLayout{kMapLowerDe, kMapUpperDe, kCtrlDe};
    return &kDeLayout;
  }
  return nullptr;
}

constexpr uint32_t kKeyBg = 0x3A3A3A;
constexpr uint32_t kKeyBgCtrl = 0x343034;      // RGB565-neutrales Mittelgrau fuer Steuertasten
constexpr uint32_t kKeyBgPressed = 0x5A5A5A;
// Gruen wie die anderen Bestaetigen-Aktionen (Verbinden/Speichern/Update)
constexpr uint32_t kKeyBgOk = 0x2E7D32;
constexpr uint32_t kKeyBgOkPressed = 0x43A047;
constexpr uint32_t kKeyText = 0xEDEDED;

// Buttonmatrix-Items kennen keine Einzel-Styles, und LVGLs Default-Theme legt
// auf die gerade fokussierte Taste (waehrend des Tippens staendig wechselnd)
// einen eigenen Outline-Ring in Akzentfarbe - dessen State-Selektor ist
// spezifischer als unser einfacher (State-loser) lokaler Style und gewinnt
// daher trotz lokalem Style (LVGL waehlt den spezifischsten State-Treffer,
// nicht "lokal schlaegt immer Theme"). Das erzeugte den gemeldeten Gruenstich.
// Deshalb werden ALLE Tastenfarben UND jeglicher Rahmen/Outline beim Zeichnen
// per Draw-Task-Event erzwungen: Buchstaben dunkelgrau, Steuertasten dunkler,
// OK-Taste gruen, Text weiss, kein Rahmen/Outline auf irgendeiner Taste.
void kb_draw_task_cb(lv_event_t* e) {
  lv_draw_task_t* task = lv_event_get_draw_task(e);
  if (!task) return;
  lv_draw_dsc_base_t* base =
      static_cast<lv_draw_dsc_base_t*>(lv_draw_task_get_draw_dsc(task));
  if (!base || base->part != LV_PART_ITEMS) return;

  lv_obj_t* kb = static_cast<lv_obj_t*>(lv_event_get_target(e));
  const uint32_t id = base->id1;
  const char* txt = lv_buttonmatrix_get_button_text(kb, id);

  lv_draw_label_dsc_t* label = lv_draw_task_get_label_dsc(task);
  if (label) {
    label->color = lv_color_hex(kKeyText);
    // Sondertasten (Pfeile, Backspace, Enter, Tastatur-Wechsel, OK) sind
    // LV_SYMBOL_*-Codepoints aus LVGLs privatem Unicode-Bereich (UTF-8-
    // Leadbyte 0xEF) - unsere ui_font_20/24 haben nur Latein/Umlaute und
    // zeigen dafuer Platzhalter-Kaestchen. Fuer diese Tasten auf LVGLs
    // eingebautes Montserrat wechseln, das den Symbolbereich mitbringt.
    if (txt && static_cast<unsigned char>(txt[0]) == 0xEF) {
#if defined(DEVICE_LAYOUT_1024X600)
      label->font = &lv_font_montserrat_20;
#else
      const bool large =
          lv_display_get_horizontal_resolution(nullptr) >= 1024;
      label->font =
          large ? &lv_font_montserrat_24 : &lv_font_montserrat_20;
#endif
    }
    return;
  }

  // Border UND Outline laufen beide als LV_DRAW_TASK_TYPE_BORDER; beide
  // komplett kappen, unabhaengig davon welcher Theme-Zustand sie ausgeloest hat.
  lv_draw_border_dsc_t* border = lv_draw_task_get_border_dsc(task);
  if (border) {
    border->opa = LV_OPA_TRANSP;
    return;
  }

  // Theme legt auf jede Taste zusaetzlich einen eigenen Box-Shadow (grauer
  // Schlagschatten mit Y-Versatz), dessen State-Selektor ebenfalls
  // spezifischer sein kann als unser lokaler shadow_opa-Style - deshalb wie
  // Border/Outline hart kappen statt sich auf die Style-Kaskade zu verlassen.
  lv_draw_box_shadow_dsc_t* shadow = lv_draw_task_get_box_shadow_dsc(task);
  if (shadow) {
    shadow->opa = LV_OPA_TRANSP;
    return;
  }

  lv_draw_fill_dsc_t* fill = lv_draw_task_get_fill_dsc(task);
  if (!fill) return;

  const bool is_ok = txt && strcmp(txt, LV_SYMBOL_OK) == 0;
  const bool is_ctrl =
      lv_buttonmatrix_has_button_ctrl(kb, id, LV_BUTTONMATRIX_CTRL_CHECKED);
  const bool pressed = lv_obj_has_state(kb, LV_STATE_PRESSED) &&
                       lv_buttonmatrix_get_selected_button(kb) == id;

  uint32_t color;
  if (is_ok) {
    color = pressed ? kKeyBgOkPressed : kKeyBgOk;
  } else if (pressed) {
    color = kKeyBgPressed;
  } else if (is_ctrl) {
    color = kKeyBgCtrl;
  } else {
    color = kKeyBg;
  }
  fill->color = lv_color_hex(color);
  fill->opa = LV_OPA_COVER;
  fill->grad.dir = LV_GRAD_DIR_NONE;
  fill->grad.stops[0].color = fill->color;
  fill->grad.stops[1].color = fill->color;
  fill->grad.stops[0].opa = LV_OPA_COVER;
  fill->grad.stops[1].opa = LV_OPA_COVER;
}

}  // namespace

lv_obj_t* ui_keyboard_create(lv_obj_t* parent) {
  lv_obj_t* kb = lv_keyboard_create(parent);

  // Rahmenlos und transparent: der Karten-Hintergrund scheint zwischen den
  // Tasten durch, die Tasten reichen bis an den Kartenrand.
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_set_style_pad_gap(kb, popup_layout::scale(6), 0);
  lv_obj_set_style_radius(kb, 0, 0);

  // Eigene Fonts statt LVGLs eingebauter Montserrat-Fonts: letztere decken nur
  // ASCII + Symbole ab, unsere ui_font_20/24 auch oe/ae/ue/ss (Bereich 174-383).
#if defined(DEVICE_LAYOUT_1024X600)
  lv_obj_set_style_text_font(kb, popup_layout::font24(), LV_PART_ITEMS);
#else
  const bool large = lv_display_get_horizontal_resolution(nullptr) >= 1024;
  lv_obj_set_style_text_font(
      kb, large ? &ui_font_24 : &ui_font_20, LV_PART_ITEMS);
#endif

  // Ohne Layout-Eintrag bleibt LVGLs eingebaute Englisch-Map (bereits nach
  // lv_keyboard_create aktiv) unveraendert.
  const DeviceConfig& kb_cfg = configManager.getConfig();
  const KeyboardLayout* layout = layout_for_config(kb_cfg.keyboard_layout, kb_cfg.language);
  if (layout) {
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, layout->lower_map, layout->ctrl_map);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, layout->upper_map, layout->ctrl_map);
  }

  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBg), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(kKeyText), LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, popup_layout::scale(10), LV_PART_ITEMS);
  lv_obj_set_style_border_opa(kb, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_outline_opa(kb, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_shadow_opa(kb, LV_OPA_TRANSP, LV_PART_ITEMS);
  // Steuertasten tragen im Keyboard-Map LV_BUTTONMATRIX_CTRL_CHECKED.
  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBgCtrl),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBgPressed),
                            LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBgPressed),
                            LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_PRESSED);

  lv_obj_add_flag(kb, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
  lv_obj_add_event_cb(kb, kb_draw_task_cb, LV_EVENT_DRAW_TASK_ADDED, nullptr);

  // Vergroesserte Tasten-Vorschau beim Halten (wie Android/iOS); die
  // POPOVER-Flags in den Control-Maps oben werden ohne diesen Aufruf von
  // LVGL wieder entfernt.
  lv_keyboard_set_popovers(kb, true);
  return kb;
}

void ui_keyboard_set_target(lv_obj_t* kb, lv_obj_t* ta, lv_obj_t* prev) {
  if (!kb || !ta) return;
  lv_keyboard_set_textarea(kb, ta);
  // Fokus-Zustand manuell setzen: nur so zeigt die Textarea ihren Cursor,
  // auch wenn der Nutzer direkt auf der Tastatur tippt statt ins Feld.
  if (prev && prev != ta) lv_obj_remove_state(prev, LV_STATE_FOCUSED);
  lv_obj_add_state(ta, LV_STATE_FOCUSED);
}
