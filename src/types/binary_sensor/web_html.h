#pragma once

#include <Arduino.h>
#include <vector>

void append_binary_sensor_fields_html(
    String& html, const String& tab_id,
    const std::vector<String>& binary_sensor_options);
