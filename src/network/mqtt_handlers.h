#ifndef MQTT_HANDLERS_H
#define MQTT_HANDLERS_H

#include <Arduino.h>

// MQTT Callback-Funktionen
void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
void mqttSubscribeTopics();
void mqttPublishDiscovery();
void mqttPublishScene(const char* scene_name);
void mqttPublishSwitchCommand(const char* entity_id, const char* state);
void mqttPublishMediaCommand(const char* entity_id, const char* command);
void mqttPublishLightCommand(const char* entity_id,
                             const char* state,
                             int brightness_pct,
                             bool has_color,
                             uint32_t color,
                             int color_temp_kelvin = -1);
void mqttPublishHistoryRequest(const char* entity_id,
                               uint16_t hours = 24,
                               uint16_t period_minutes = 5,
                               uint16_t points = 288);
void mqttPublishWeatherRequest(const char* entity_id);
bool mqttPublishEnergyRequest(const char* period = "day");
void mqttPublishHomeSnapshot();
void mqttPublishDeviceSettings();
void mqttServiceLocalSensors();
void mqttReloadDynamicSlots();

#endif // MQTT_HANDLERS_H
