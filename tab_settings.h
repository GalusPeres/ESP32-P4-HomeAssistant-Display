#pragma once
#include <lvgl.h>

// Callback-Typ für Hotspot-Button
typedef void (*hotspot_callback_t)();

void build_settings_tab(lv_obj_t *tab, hotspot_callback_t hotspot_cb = nullptr);

// Update WiFi-Status (aufgerufen von main loop)
void settings_update_wifi_status(bool connected, const char* ssid, const char* ip);
void settings_show_mqtt_warning(bool show);

// Update Power-Status (aufgerufen von main loop)
void settings_update_power_status();
