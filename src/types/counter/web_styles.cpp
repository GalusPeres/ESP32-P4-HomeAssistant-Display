#include "src/types/counter/web_styles.h"

void append_counter_styles(String& html) {
  html += R"html(
  <style>
    .tile.counter { display:grid; grid-template-rows:auto 1fr; grid-template-columns:1fr; }
    .tile.counter .tile-title { text-align:left; align-self:start; width:100%; }
    .tile.counter .tile-counter-value {
      color:#fff;
      font-size:28px;
      font-weight:bold;
      text-align:center;
      align-self:center;
      justify-self:center;
    }
  </style>
)html";
}
