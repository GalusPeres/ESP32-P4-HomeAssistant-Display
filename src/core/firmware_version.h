#pragma once

#include "version.txt"

#if defined(DEVICE_GUITION_JC8012P4A1) && \
    defined(HOMETILES_JC8012_C6_RECOVERY)
#undef FW_VERSION
#define FW_VERSION "v0.6.8b2"
#endif

#ifndef FW_VERSION
#error "FW_VERSION is missing in version.txt"
#endif
