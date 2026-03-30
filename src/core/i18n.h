#ifndef I18N_H
#define I18N_H

#include <Arduino.h>

namespace i18n {

struct Strings {
  const char* code;
  const char* html_lang;
  const char* language_label;
  const char* timezone_label;
  const char* language_option_english;
  const char* language_option_german;

  const char* home;
  const char* folder_prefix;

  const char* admin_window_title;
  const char* admin_panel_title;
  const char* admin_subtitle;
  const char* admin_tile_hint;
  const char* admin_delete_folder_tab;
  const char* admin_tile_settings;
  const char* admin_type;
  const char* admin_title;
  const char* admin_tile_title_placeholder;
  const char* admin_icon_label;
  const char* admin_icon_placeholder;
  const char* admin_icon_list;
  const char* admin_color;
  const char* admin_column;
  const char* admin_row;
  const char* admin_width_cells;
  const char* admin_height_cells;
  const char* admin_autosave;
  const char* admin_copy;
  const char* admin_paste;
  const char* admin_delete;
  const char* admin_import_export;
  const char* admin_export;
  const char* admin_import;
  const char* admin_import_overwrite;
  const char* admin_settings_wifi;
  const char* admin_settings_mqtt;
  const char* admin_settings_language;
  const char* admin_settings_screenshot;
  const char* screenshot_create_download;
  const char* screenshot_saved_note;

  const char* wifi_status;
  const char* wifi_connected;
  const char* wifi_disconnected;
  const char* wifi_offline;
  const char* wifi_status_ap_format;
  const char* wifi_label;
  const char* ssid_label;
  const char* ip_label;
  const char* wifi_password_label;
  const char* wifi_static_ip_label;
  const char* wifi_gateway_label;
  const char* wifi_subnet_label;
  const char* wifi_dns_label;
  const char* wifi_dhcp_hint;
  const char* mqtt_not_configured;
  const char* ap_enable;
  const char* ap_disable;
  const char* yes;
  const char* no;

  const char* mqtt_host;
  const char* mqtt_port;
  const char* mqtt_username;
  const char* mqtt_password;
  const char* mqtt_client_id;
  const char* mqtt_client_id_placeholder;
  const char* mqtt_client_id_hint;
  const char* mqtt_base_topic;
  const char* ha_prefix;
  const char* status_time_font;
  const char* status_date_font;
  const char* save;
  const char* restart_confirm;
  const char* restart_button;
  const char* mqtt_saved_title;
  const char* mqtt_saved_message;
  const char* bridge_saved_title;
  const char* bridge_saved_message;
  const char* save_failed;

  const char* ap_window_title;
  const char* ap_heading;
  const char* ap_subtitle;
  const char* ap_wifi_section;
  const char* ap_wifi_ssid_label;
  const char* ap_wifi_ssid_placeholder;
  const char* ap_wifi_password_label;
  const char* ap_wifi_password_placeholder;
  const char* ap_wifi_open_hint;
  const char* ap_info_notice;
  const char* ap_info_message;
  const char* ap_save_connect;
  const char* ap_success_title;
  const char* ap_success_message;
  const char* ap_success_notice;
  const char* ap_wifi_required;

  const char* display_label;
  const char* brightness_label;
  const char* sleep_label;
  const char* sleep_after;
  const char* sleep_never;
  const char* touch_label;
  const char* no_imu_hint;

  const char* tile_type_empty;
  const char* tile_type_sensor;
  const char* tile_type_weather;
  const char* tile_type_scene;
  const char* tile_type_key;
  const char* tile_type_folder;
  const char* tile_type_switch;
  const char* tile_type_clock;
  const char* tile_type_text;
  const char* tile_type_counter;
  const char* tile_type_settings;
  const char* tile_type_back;

  const char* no_selection;
  const char* sensor_entity;
  const char* sensor_unit;
  const char* sensor_decimals;
  const char* sensor_value_size;
  const char* sensor_display_mode;
  const char* sensor_display_none;
  const char* sensor_display_gauge;
  const char* sensor_display_graph;
  const char* sensor_gauge_min;
  const char* sensor_gauge_max;
  const char* sensor_arc_degree;
  const char* sensor_gauge_size;
  const char* sensor_y_offset;
  const char* sensor_graph_height;
  const char* popup_open;
  const char* short_press;
  const char* long_press;
  const char* sensor_value_y_offset;
  const char* weather_entity;
  const char* switch_light;
  const char* switch_display;
  const char* switch_icon_button;
  const char* switch_lvgl_switch;
  const char* show_time;
  const char* show_date;
  const char* time_font_size;
  const char* date_font_size;
  const char* text_label;
  const char* text_placeholder;
  const char* text_size;
  const char* text_max_chars;
  const char* target_folder;
  const char* new_folder;
  const char* scene_label;
  const char* macro_label;
  const char* macro_examples;
  const char* counter_start_value;
  const char* counter_hint;

  const char* js_select_tile_first;
  const char* js_tile_copied;
  const char* js_no_copied_tile;
  const char* js_tile_pasted;
  const char* js_settings_tile_fixed;
  const char* js_back_tile_fixed;
  const char* js_tile_cannot_delete;
  const char* js_folder_cannot_delete;
  const char* js_delete_folder_confirm;
  const char* js_folder_deleted;
  const char* js_delete_failed;
  const char* js_folder_not_found;
  const char* js_tile_saved;
  const char* js_unknown_error;
  const char* js_network_error;
  const char* js_network_error_save;
  const char* js_export_created;
  const char* js_export_failed;
  const char* js_import_invalid_json;
  const char* js_import_failed;
  const char* js_import_running;
  const char* js_import_complete;
  const char* js_tile_does_not_fit;
  const char* js_no_layout_found;
  const char* js_tiles_moved_saved;
  const char* js_move_failed;
  const char* js_network_error_move;
  const char* js_screenshot_creating;
  const char* js_screenshot_saved;
  const char* js_screenshot_failed;
};

const Strings& strings(const char* language_code);
const char* normalize_language_code(const char* language_code);
String build_language_options_html(const char* selected_code);
String weather_condition_label(const char* language_code, const String& condition);
String weather_weekday_short(const char* language_code, const String& iso);
const char* weather_tomorrow_label(const char* language_code);

}  // namespace i18n

#endif
