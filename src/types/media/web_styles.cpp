#include "src/types/media/web_styles.h"

void append_media_styles(String& html) {
  html += R"html(
  <style>
    .tile.media { position:relative; overflow:hidden; }
    .tile.media .tile-icon {
      position:absolute;
      top:4px;
      left:6px;
    }
    .tile.media .tile-title {
      position:absolute;
      top:8px;
      right:8px;
      text-align:right;
      max-width:60%;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.media .tile-media-title {
      position:absolute;
      left:10px;
      right:10px;
      top:45%;
      transform:translateY(-50%);
      text-align:center;
      font-weight:700;
      white-space:nowrap;
      overflow:hidden;
      text-overflow:ellipsis;
    }
    .tile.media .tile-media-subtitle {
      position:absolute;
      left:12px;
      right:12px;
      top:62%;
      text-align:center;
      color:#d8dee9;
      font-size:12px;
      white-space:nowrap;
      overflow:hidden;
      text-overflow:ellipsis;
    }
    .tile.media .tile-media-state {
      position:absolute;
      right:10px;
      bottom:8px;
      color:#b7c1d6;
      font-size:11px;
      max-width:70%;
      white-space:nowrap;
      overflow:hidden;
      text-overflow:ellipsis;
    }
  </style>
)html";
}
