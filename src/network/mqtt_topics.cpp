#include "src/network/mqtt_topics.h"

MqttTopicRegistry mqttTopics;

const MqttTopicRegistry::TopicDescriptor MqttTopicRegistry::kDescriptors[] = {
  {TopicKey::SENSOR_OUT, TopicDomain::Sensor, "outside_c"},
  {TopicKey::SENSOR_IN, TopicDomain::Sensor, "inside_c"},
  {TopicKey::SENSOR_SOC, TopicDomain::Sensor, "soc_pct"},
  {TopicKey::SCENE_CMND, TopicDomain::Command, "scene"},
  {TopicKey::STAT_CONN, TopicDomain::State, "connected"},
  {TopicKey::TELE_UP, TopicDomain::Telemetry, "uptime"},
  {TopicKey::HA_WOHN_TEMP, TopicDomain::HaStatestream, "sensor/og_wohnbereich_sensor_temperatur/state"},
  {TopicKey::HA_PV_GARAGE, TopicDomain::HaStatestream, "sensor/pv_haus_garage/state"},
  {TopicKey::HA_PV_GARAGE_HISTORY, TopicDomain::HaStatestream, "sensor/pv_haus_garage/history"},
};

void MqttTopicRegistry::begin(const TopicSettings& settings) {
  if (settings.device_base.length()) {
    device_base_ = settings.device_base;
  } else if (device_base_.isEmpty()) {
    device_base_ = "tab5";
  }

  if (settings.ha_prefix.length()) {
    ha_prefix_ = settings.ha_prefix;
  } else if (ha_prefix_.isEmpty()) {
    ha_prefix_ = "ha/statestream";
  }

  while (device_base_.endsWith("/")) {
    device_base_.remove(device_base_.length() - 1);
  }
  while (ha_prefix_.endsWith("/")) {
    ha_prefix_.remove(ha_prefix_.length() - 1);
  }

  sensor_root_ = device_base_ + "/sensor/";
  command_root_ = device_base_ + "/cmnd/";
  state_root_ = device_base_ + "/stat/";
  telemetry_root_ = device_base_ + "/tele/";

  buildTopics();
}

const char* MqttTopicRegistry::topic(TopicKey key) const {
  size_t idx = static_cast<size_t>(key);
  if (idx >= static_cast<size_t>(TopicKey::COUNT)) {
    return nullptr;
  }
  return topics_[idx].c_str();
}

void MqttTopicRegistry::buildTopics() {
  for (const auto& desc : kDescriptors) {
    size_t idx = static_cast<size_t>(desc.key);
    topics_[idx] = buildPath(desc.domain, desc.leaf);
  }
}

String MqttTopicRegistry::buildPath(TopicDomain domain, const char* leaf) const {
  switch (domain) {
    case TopicDomain::Sensor:
      return sensor_root_ + leaf;
    case TopicDomain::Command:
      return command_root_ + leaf;
    case TopicDomain::State:
      return state_root_ + leaf;
    case TopicDomain::Telemetry:
      return telemetry_root_ + leaf;
    case TopicDomain::HaStatestream:
      return ha_prefix_ + "/" + leaf;
  }
  return "";
}
