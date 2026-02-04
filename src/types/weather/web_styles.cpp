#include "src/types/weather/web_styles.h"

void append_weather_styles(String& html) {
  html += R"html(
  <style>
    .tile.weather { display:flex; flex-direction:column; align-items:center; justify-content:center; gap:4px; }
  </style>
)html";
}
