#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "src/tiles/tile_renderer.h"

struct CoverPopupInit {
  String entity_id;
  String title;
  String icon_name;
  CoverState state;
  bool icon_visible = true;
};

void show_cover_popup(const CoverPopupInit& init);
void update_cover_popup(const CoverPopupInit& init);
void preload_cover_popup();
void hide_cover_popup();
