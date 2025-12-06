#ifndef GAME_CONTROLS_CONFIG_H
#define GAME_CONTROLS_CONFIG_H

#include <Arduino.h>

static constexpr size_t GAME_BUTTON_COUNT = 12;

struct GameButton {
  String name;          // "Landing Gear", "Lights", etc.
  uint8_t key_code;     // KEY_N, KEY_L, etc. (0 = leer)
  uint8_t modifier;     // 0 oder KEY_LEFT_CTRL, KEY_LEFT_ALT, KEY_LEFT_SHIFT
};

struct GameControlsConfigData {
  GameButton buttons[GAME_BUTTON_COUNT];
};

class GameControlsConfig {
public:
  GameControlsConfig();

  bool load();
  bool save(const GameControlsConfigData& data);

  const GameControlsConfigData& get() const { return data; }
  bool hasData() const;

private:
  GameControlsConfigData data;
};

extern GameControlsConfig gameControlsConfig;

#endif // GAME_CONTROLS_CONFIG_H
