#ifndef MQTT_HANDLERS_H
#define MQTT_HANDLERS_H

#include <Arduino.h>

// MQTT Callback-Funktionen
void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
// Drains inbound MQTT messages that mqttCallback() queued (see mqtt_handlers.cpp
// header comment) and runs the real per-topic processing on the caller's task.
// Call from the main loop(). max_msgs=0 drains everything currently queued.
void mqtt_process_inbound_queue(uint8_t max_msgs = 0);
// Konsumiert das Post-Connect-Pending-Flag des MQTT-Workers und faehrt die
// App-Ebene hoch (Subscribes/Discovery/Settings/Snapshot). Muss auf dem
// Loop-Task laufen (Flash/LVGL/I2C); jede Loop-Iteration aufrufen.
void mqttServicePostConnect();
void mqttSubscribeTopics();
void mqttPublishDiscovery();
void mqttPublishScene(const char* scene_name);
void mqttPublishSwitchCommand(const char* entity_id, const char* state);
void mqttPublishMediaCommand(const char* entity_id, const char* command);
void mqttPublishMediaSeek(const char* entity_id, float position_seconds);
void mqttPublishMediaVolume(const char* entity_id, float volume_level);
void mqttPublishMediaMute(const char* entity_id, bool muted);
void mqttPublishCameraCommand(const char* entity_id, const char* command);
void mqttPublishClimateTemperature(const char* entity_id,
                                   float temperature,
                                   bool use_range = false,
                                   float target_low = 0.0f,
                                   float target_high = 0.0f);
void mqttPublishClimateHumidity(const char* entity_id, float humidity);
void mqttPublishClimateHvacMode(const char* entity_id, const char* hvac_mode);
void mqttPublishClimatePresetMode(const char* entity_id, const char* preset_mode);
void mqttPublishClimateFanMode(const char* entity_id, const char* fan_mode);
void mqttPublishClimateSwingMode(const char* entity_id, const char* swing_mode);
void mqttPublishClimateHorizontalSwingMode(
    const char* entity_id, const char* swing_mode);
void mqttPublishCoverCommand(const char* entity_id,
                             const char* command,
                             int position = -1);
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
void mqttReloadDynamicSlots(bool subscribe_all = false);
void mqttRequestDynamicSlotsReload(uint32_t quiet_ms = 3000);
void mqttServiceDynamicSlotsReload();
// Boot-Scan fuer setup(): Media-Tile in der gespeicherten Config? Ergebnis
// steuert die Startgroesse des MQTT-Empfangspuffers (Cover-Payloads ~19 KB).
bool mqttAnyMediaTileConfigured();

#endif // MQTT_HANDLERS_H
