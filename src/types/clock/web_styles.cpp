#include "src/types/clock/web_styles.h"

void append_clock_styles(String& html) {
  html += R"html(
  <style>
    .tile.clock { display:flex; flex-direction:column; align-items:center; justify-content:center; gap:4px; }
    .tile.clock .tile-title {
      position:absolute;
      top:8px;
      left:8px;
      text-align:left;
      width:auto;
      margin:0;
    }
    .tile.clock .tile-icon {
      position:absolute;
      top:8px;
      right:8px;
      margin:0;
    }
    .tile-clock-time {
      color:#fff;
      font-size:28px;
      font-weight:600;
      line-height:1;
    }
    .tile-clock-date {
      color:#cbd5e1;
      font-size:16px;
      font-weight:500;
      line-height:1.1;
    }
  </style>
)html";
}
