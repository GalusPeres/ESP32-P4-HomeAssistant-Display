#include "src/types/camera/web_styles.h"

void append_camera_styles(String& html) {
  html += R"html(
  <style>
    .tile.camera { position:relative; overflow:hidden; }
    .tile.camera .tile-icon { position:absolute; top:4px; left:6px; }
    .tile.camera .tile-title {
      position:absolute;
      top:8px;
      right:8px;
      max-width:70%;
      overflow:hidden;
      text-align:right;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
  </style>
)html";
}
