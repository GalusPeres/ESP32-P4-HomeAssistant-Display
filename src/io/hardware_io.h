#pragma once

#include <Arduino.h>

#include "src/devices/hardware_io_profile.h"

static constexpr uint8_t kHardwareIoMaxChannels = 8;

enum class HardwareIoType : uint8_t {
  Relay = 0,
  Temperature = 1,
};

struct HardwareIoChannelConfig {
  String id;
  String name;
  HardwareIoType type = HardwareIoType::Relay;
  int8_t gpio = -1;
  bool inverted = false;
  bool boot_on = false;
  uint8_t precision = 1;
};

// Bounded hardware-I/O runtime. Relays switch directly; DS18x20 sensors use
// a stepped, asynchronous 1-Wire state machine. The normal render/touch path
// performs no allocations or flash reads.
class HardwareIoManager {
 public:
  HardwareIoManager();

  bool load();
  void begin();
  void service();

  String toJson(bool include_device_meta = false) const;
  bool replaceFromJson(const String& json, String& error);

  void appendBridgeJson(String& json) const;
  void subscribeMqttTopics();
  void publishAllStates();
  void refreshLocalEntityCache();
  bool handleMqttMessage(const char* topic, const uint8_t* payload,
                         size_t length);

  // Physical channels share the same short visible entity ID on the panel
  // and in Home Assistant: switch.<device>_<name> or sensor.<device>_<name>.
  // The internal channel ID stays stable for MQTT and HA unique_id, which also
  // contains the full device ID. HA can append _2/_3 for duplicate panels.
  // Panel control works directly without MQTT or Bridge.
  bool localEntityInfo(uint8_t index, String& entity_id, String& name,
                       HardwareIoType& type) const;
  bool isLocalEntityId(const char* entity_id) const;
  bool handleLocalEntityCommand(const char* entity_id, const char* action);

  uint8_t channelCount() const { return channel_count_; }
  bool hasTemperatureChannels() const;

  static const Device::HardwareIoPinOption* pinOptions(size_t& count);

 private:
  struct RuntimeChannel {
    bool relay_state = false;
    bool temperature_valid = false;
    bool conversion_pending = false;
    bool scratchpad_reading = false;
    uint8_t failures = 0;
    uint8_t scratchpad_index = 0;
    uint8_t scratchpad[9] = {0};
    float temperature_c = NAN;
    uint32_t conversion_started_ms = 0;
    uint32_t next_action_ms = 0;
    uint32_t last_publish_ms = 0;
    String command_topic;
    String state_topic;
    String local_entity_id;
    char last_payload[24] = {0};
    char last_local_payload[24] = {0};
  };

  HardwareIoChannelConfig channels_[kHardwareIoMaxChannels];
  RuntimeChannel runtime_[kHardwareIoMaxChannels];
  uint8_t channel_count_ = 0;
  uint8_t service_cursor_ = 0;
  uint8_t stale_state_topic_count_ = 0;
  bool begun_ = false;
  String board_variant_;
  uint32_t stale_state_retry_ms_ = 0;
  String stale_state_topics_[kHardwareIoMaxChannels];

  void reset();
  bool loadPath(const char* path);
  bool parseDocument(const String& json,
                     HardwareIoChannelConfig* out_channels,
                     uint8_t& out_count, String& out_board_variant,
                     String& error, bool allow_missing_names) const;
  bool saveChannels(const HardwareIoChannelConfig* channels,
                    uint8_t count, const String& board_variant) const;
  void stopRuntime();
  void startRuntime();
  void transitionRuntime(const HardwareIoChannelConfig* channels,
                         uint8_t count);
  void rebuildMqttTopics();
  void unsubscribeMqttTopics();
  void rememberStaleStateTopic(const String& topic);
  void flushStaleStateTopics();
  void applyRelay(uint8_t index, bool on, bool publish);
  String localEntityId(uint8_t index) const;
  void buildChannelPayload(uint8_t index, char* payload,
                           size_t payload_size) const;
  void markLocalEntityUnavailable(const HardwareIoChannelConfig& config,
                                  const String& entity_id);
  void syncLocalEntityState(uint8_t index, const char* payload, bool force);
  void syncAllLocalEntityStates(bool force);
  void publishChannelState(uint8_t index, bool force);
  bool serviceTemperature(uint8_t index, uint32_t now_ms);
};

extern HardwareIoManager hardwareIo;
