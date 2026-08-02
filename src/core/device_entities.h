#ifndef DEVICE_ENTITIES_H
#define DEVICE_ENTITIES_H

#include <Arduino.h>

static constexpr const char* kEntityDisplayBrightness = "light.tab5_display_brightness";
static constexpr const char* kEntityScreensaverBrightness =
    "light.tab5_screensaver_brightness";
static constexpr const char* kEntityDisplayRotate = "switch.tab5_display_rotate";
static constexpr const char* kEntityDisplaySleep = "switch.tab5_display_sleep";
static constexpr const char* kEntityInternalBatterySoc = "sensor.tab5_internal_battery_soc";
static constexpr const char* kEntityExternalTemperature = "sensor.tab5_external_temperature";

#endif  // DEVICE_ENTITIES_H
