#pragma once

#include <Arduino.h>

static constexpr uint8_t kHardwareIoMaxChannels = 8;

enum class HardwareIoType : uint8_t {
  Relay = 0,
  Temperature = 1,
};

enum HardwareIoPinCapability : uint8_t {
  HARDWARE_IO_PIN_RELAY = 1U << 0,
  HARDWARE_IO_PIN_TEMPERATURE = 1U << 1,
  HARDWARE_IO_PIN_ONBOARD = 1U << 2,
};

struct HardwareIoPinOption {
  int8_t gpio;
  uint8_t capabilities;
  const char* label;
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

// Persistente, bewusst kleine Hardware-I/O-Laufzeit. Relais werden direkt
// geschaltet; DS18x20-Sensoren laufen als gestaffelte, asynchrone 1-Wire-
// Zustandsmaschine. Im normalen Render-/Touch-Pfad wird nichts alloziert und
// kein Flash gelesen.
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
  bool handleMqttMessage(const char* topic, const uint8_t* payload,
                         size_t length);

  uint8_t channelCount() const { return channel_count_; }
  bool hasTemperatureChannels() const;

  static const HardwareIoPinOption* pinOptions(size_t& count);
  static bool pinSupports(int gpio, HardwareIoType type,
                          bool allow_onboard_relays = false);

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
    char last_payload[24] = {0};
  };

  HardwareIoChannelConfig channels_[kHardwareIoMaxChannels];
  RuntimeChannel runtime_[kHardwareIoMaxChannels];
  uint8_t channel_count_ = 0;
  uint8_t service_cursor_ = 0;
  uint8_t stale_state_topic_count_ = 0;
  bool begun_ = false;
  bool board_variant_86_2ro_ = false;
  uint32_t stale_state_retry_ms_ = 0;
  String stale_state_topics_[kHardwareIoMaxChannels];

  void reset();
  bool loadPath(const char* path);
  bool parseDocument(const String& json,
                     HardwareIoChannelConfig* out_channels,
                     uint8_t& out_count, bool& out_board_variant_86_2ro,
                     String& error) const;
  bool saveChannels(const HardwareIoChannelConfig* channels,
                    uint8_t count, bool board_variant_86_2ro) const;
  void stopRuntime();
  void startRuntime();
  void transitionRuntime(const HardwareIoChannelConfig* channels,
                         uint8_t count);
  void rebuildMqttTopics();
  void unsubscribeMqttTopics();
  void rememberStaleStateTopic(const String& topic);
  void flushStaleStateTopics();
  void applyRelay(uint8_t index, bool on, bool publish);
  void publishChannelState(uint8_t index, bool force);
  bool serviceTemperature(uint8_t index, uint32_t now_ms);
};

extern HardwareIoManager hardwareIo;
