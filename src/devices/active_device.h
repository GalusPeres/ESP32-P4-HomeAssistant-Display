#pragma once

#include "src/devices/device_select.h"

#if defined(DEVICE_M5STACKS_TAB5)
#include "src/devices/m5stacks_tab5/device_m5stacks_tab5.h"
namespace DeviceImpl = DeviceM5StacksTab5;
#elif defined(DEVICE_WAVESHARE_4B)
#include "src/devices/waveshare_4b/device_waveshare_4b.h"
namespace DeviceImpl = DeviceWaveshare4B;
#else
#error "No supported device target selected."
#endif
