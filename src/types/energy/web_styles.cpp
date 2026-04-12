#include "src/types/energy/web_styles.h"

void append_energy_styles(String& html) {
  html += R"html(
  <style>
    .tile.energy { display:grid; grid-template-rows:auto 1fr; grid-template-columns:1fr; }
    .tile.energy .tile-title {
      text-align:right;
      align-self:start;
      width:auto;
      grid-column:1;
      grid-row:1;
      justify-self:end;
      max-width:60%;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.energy .tile-icon {
      position:absolute;
      top:4px;
      left:6px;
    }
  </style>
)html";
}

