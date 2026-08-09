#pragma once

#include <Arduino.h>
#include <vector>

void append_cover_fields_html(String& html, const String& tab_id,
                              const std::vector<String>& cover_options);
