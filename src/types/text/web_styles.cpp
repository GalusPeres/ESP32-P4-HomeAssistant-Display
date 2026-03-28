#include "src/types/text/web_styles.h"

void append_text_styles(String& html) {
  html += R"html(
  <style>
    .tile.text { display:grid; grid-template-rows:auto 1fr; grid-template-columns:1fr; }
    .tile.text .tile-title { text-align:left; align-self:start; width:100%; }
    .tile.text .tile-text {
      color:#fff;
      font-size:16px;
      text-align:center;
      white-space:pre-wrap;
      line-height:1.2;
      align-self:center;
      justify-self:center;
      padding:0 8px;
      overflow:hidden;
    }
    .tile.text .tile-text.sensor-value-size-default { font-size:28px; }
    .tile.text .tile-text.sensor-value-size-32 { font-size:32px; }
    .tile.text .tile-text.sensor-value-size-28 { font-size:28px; }
    .tile.text .tile-text.sensor-value-size-24 { font-size:24px; }
    .tile.text .tile-text.sensor-value-size-20 { font-size:20px; }
    .tile.text .tile-icon { position:absolute; top:10px; right:8px; }
    .tile-settings textarea {
      width:100%;
      padding:10px 12px;
      border:1px solid #cbd5f0;
      border-radius:10px;
      font-size:14px;
      font-family:inherit;
      box-sizing:border-box;
      resize:vertical;
      margin-bottom:12px;
    }
  </style>
)html";
}
