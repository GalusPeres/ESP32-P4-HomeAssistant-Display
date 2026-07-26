#include "src/types/camera/web_styles.h"

void append_camera_styles(String& html) {
  html += R"html(
  <style>
    .tile.camera { display:flex; flex-direction:column; align-items:center; justify-content:center; }
    .tile.camera .tile-title { text-align:center; align-self:auto; width:100%; margin-top:4px; }
    .tile.camera .tile-icon { margin-bottom:4px; }
  </style>
)html";
}
