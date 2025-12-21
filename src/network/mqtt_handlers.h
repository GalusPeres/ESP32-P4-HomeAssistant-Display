#ifndef MQTT_HANDLERS_H
#define MQTT_HANDLERS_H

#include <Arduino.h>

// MQTT Callback-Funktionen
void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
void mqttSubscribeTopics();
void mqttPublishDiscovery();
void mqttPublishScene(const char* scene_name);
void mqttPublishSwitchCommand(const char* entity_id, const char* state);
void mqttPublishLightCommand(const char* entity_id, const char* state, int brightness_pct, bool has_color, uint32_t color);
void mqttPublishHomeSnapshot();
void mqttReloadDynamicSlots();

#endif // MQTT_HANDLERS_H
