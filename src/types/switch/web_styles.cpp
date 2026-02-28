#include "src/types/switch/web_styles.h"

void append_switch_styles(String& html) {
  html += R"html(
  <style>
    .tile.switch { display:flex; flex-direction:column; align-items:center; justify-content:center; }
    .tile.switch .tile-title { text-align:center; align-self:auto; width:100%; margin-top:4px; }
    .tile.switch .tile-icon { margin-bottom:4px; }
    .tile.switch.switch-toggle { display:grid; grid-template-rows:auto 1fr; grid-template-columns:1fr; }
    .tile.switch.switch-toggle .tile-title { text-align:left; align-self:start; width:100%; margin-top:0; }
    .tile.switch.switch-toggle .tile-icon { position:absolute; top:10px; right:8px; margin:0; }
    .tile-switch {
      width:58px;
      height:28px;
      border-radius:999px;
      background:#fff;
      position:relative;
      align-self:center;
      justify-self:center;
      margin-top:10px;
      box-shadow: inset 0 0 0 1px rgba(0,0,0,0.08);
    }
    .tile-switch .tile-switch-knob {
      position:absolute;
      top:3px;
      left:3px;
      width:22px;
      height:22px;
      border-radius:50%;
      background:var(--switch-knob-color, #f8fafc);
      transition:transform 0.15s ease, background 0.15s ease;
    }
    .tile-switch.is-on { background: var(--switch-on-color, #3B82F6); }
    .tile-switch.is-on .tile-switch-knob { transform: translateX(30px); }
  </style>
)html";
}
