#ifndef MQTT_TOPICS_H
#define MQTT_TOPICS_H

#include <Arduino.h>

struct TopicSettings {
  String device_base;
  String ha_prefix;
};

enum class TopicKey : uint8_t {
  SENSOR_OUT = 0,
  SENSOR_IN,
  SENSOR_SOC,
  SCENE_CMND,
  STAT_CONN,
  TELE_UP,
  HA_WOHN_TEMP,
  HA_PV_GARAGE,
  HA_PV_GARAGE_HISTORY,
  COUNT
};

class MqttTopicRegistry {
public:
  void begin(const TopicSettings& settings);

  const char* topic(TopicKey key) const;
  const String& deviceBase() const { return device_base_; }
  const String& haPrefix() const { return ha_prefix_; }

private:
  enum class TopicDomain : uint8_t {
    Sensor,
    Command,
    State,
    Telemetry,
    HaStatestream
  };

  struct TopicDescriptor {
    TopicKey key;
    TopicDomain domain;
    const char* leaf;
  };

  String device_base_ = "tab5";
  String ha_prefix_ = "ha/statestream";
  String sensor_root_;
  String command_root_;
  String state_root_;
  String telemetry_root_;

  static const TopicDescriptor kDescriptors[];
  String topics_[static_cast<size_t>(TopicKey::COUNT)];

  void buildTopics();
  String buildPath(TopicDomain domain, const char* leaf) const;
};

extern MqttTopicRegistry mqttTopics;

#endif // MQTT_TOPICS_H
