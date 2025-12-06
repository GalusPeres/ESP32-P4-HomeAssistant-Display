#include "src/game/game_controls_config.h"
#include <Preferences.h>

static const char* PREF_NAMESPACE = "tab5_config";

GameControlsConfig gameControlsConfig;

GameControlsConfig::GameControlsConfig() = default;

bool GameControlsConfig::load() {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    return false;
  }

  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    char key[16];

    snprintf(key, sizeof(key), "game_name%u", static_cast<unsigned>(i));
    data.buttons[i].name = prefs.getString(key, "");

    snprintf(key, sizeof(key), "game_key%u", static_cast<unsigned>(i));
    data.buttons[i].key_code = prefs.getUChar(key, 0);

    snprintf(key, sizeof(key), "game_mod%u", static_cast<unsigned>(i));
    data.buttons[i].modifier = prefs.getUChar(key, 0);
  }

  prefs.end();
  return true;
}

bool GameControlsConfig::save(const GameControlsConfigData& incoming) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    return false;
  }

  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    char key[16];

    snprintf(key, sizeof(key), "game_name%u", static_cast<unsigned>(i));
    prefs.putString(key, incoming.buttons[i].name);

    snprintf(key, sizeof(key), "game_key%u", static_cast<unsigned>(i));
    prefs.putUChar(key, incoming.buttons[i].key_code);

    snprintf(key, sizeof(key), "game_mod%u", static_cast<unsigned>(i));
    prefs.putUChar(key, incoming.buttons[i].modifier);
  }

  prefs.end();
  data = incoming;

  Serial.println("[Game Controls] Konfiguration gespeichert");
  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    if (data.buttons[i].key_code) {
      Serial.printf("  [%u] %s: Key=0x%02X Mod=0x%02X\n",
                    static_cast<unsigned>(i),
                    data.buttons[i].name.c_str(),
                    data.buttons[i].key_code,
                    data.buttons[i].modifier);
    }
  }

  return true;
}

bool GameControlsConfig::hasData() const {
  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    if (data.buttons[i].key_code != 0) {
      return true;
    }
  }
  return false;
}
