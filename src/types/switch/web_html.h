#pragma once

#include <Arduino.h>
#include <vector>
#include "src/web/server/web_admin_utils.h"

void append_switch_fields_html(String& html, const String& tab_id, const std::vector<String>& switchOptions);
