#include <lvgl.h>
#include "tab_weather.h"

void build_weather_tab(lv_obj_t *tab) {
  lv_obj_set_style_bg_color(tab, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
  // Ort
  lv_obj_t *city = lv_label_create(tab);
  lv_label_set_text(city, "Bayerischer Wald");
  lv_obj_set_style_text_color(city, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_text_font(city, &lv_font_montserrat_20, 0);
  lv_obj_align(city, LV_ALIGN_TOP_MID, 0, 80);

  // große Temperatur
  lv_obj_t *temp = lv_label_create(tab);
  lv_label_set_text(temp, "12°C");
  lv_obj_set_style_text_color(temp, lv_color_white(), 0);
  lv_obj_set_style_text_font(temp, &lv_font_montserrat_48, 0);
  lv_obj_align(temp, LV_ALIGN_CENTER, 0, -20);

  // weitere Werte (Platzhalter)
  lv_obj_t *misc = lv_label_create(tab);
  lv_label_set_text(misc, "Wind 8 km/h  |  Feuchte 65%  |  Bewoelkt");
  lv_obj_set_style_text_color(misc, lv_color_hex(0xBBBBBB), 0);
  lv_obj_set_style_text_font(misc, &lv_font_montserrat_20, 0);
  lv_obj_align(misc, LV_ALIGN_CENTER, 0, 40);

  // 3 kleine Forecast-Karten
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *card = lv_obj_create(tab);
    lv_obj_set_size(card, 220, 120);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2F2F2F), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_align(card, LV_ALIGN_BOTTOM_MID, (i - 1) * 260, -40);

    lv_obj_t *lbl_day = lv_label_create(card);
    const char* days[3] = {"Heute", "Morgen", "Sa"};
    lv_label_set_text(lbl_day, days[i]);
    lv_obj_set_style_text_color(lbl_day, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_day, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_day, LV_ALIGN_TOP_LEFT, 16, 10);

    lv_obj_t *lbl_temp = lv_label_create(card);
    lv_label_set_text(lbl_temp, (i == 0) ? "12/6°C" : (i == 1) ? "9/3°C" : "7/1°C");
    lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
  }
}
