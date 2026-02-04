#include "src/types/weather/web_scripts.h"

void append_weather_scripts(String& html) {
  html += R"html(
  <script>
  function maybeFillTitleFromWeather(tab) {
    maybeFillTitleFromEntity(tab, '_weather_entity');
  }

  function loadWeatherFields(tab, data) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_weather_entity');
    if (el) el.value = data.sensor_entity || data.weather_entity || '';
    maybeFillTitleFromWeather(tab);
  }

  function saveWeatherFields(tab, formData) {
    const prefix = tab;
    formData.append('weather_entity', document.getElementById(prefix + '_weather_entity')?.value || '');
  }

  function resetWeatherFields(tab) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_weather_entity');
    if (el) el.value = '';
  }
  </script>
)html";
}
