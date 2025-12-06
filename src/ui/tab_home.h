#ifndef TAB_HOME_H
#define TAB_HOME_H

#include <lvgl.h>
#include "src/network/ha_bridge_config.h"

typedef void (*scene_publish_cb_t)(const char* scene_name);

void build_home_tab(lv_obj_t *parent, scene_publish_cb_t scene_cb);
void home_reload_layout();
void home_set_sensor_slot_value(uint8_t slot, const char* value);

#endif // TAB_HOME_H
