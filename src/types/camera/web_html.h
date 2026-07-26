#pragma once

#include <Arduino.h>
#include <vector>

void append_camera_fields_html(
    String& html,
    const String& tab_id,
    const std::vector<String>& camera_options);
