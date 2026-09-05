#ifndef GITHUB_UPDATE_H
#define GITHUB_UPDATE_H

#include <Arduino.h>

// GitHub update: check /releases/latest through its redirect (the Location
// header is sufficient, so no JSON API or API rate limit is involved), then
// download the release asset over HTTPS into the OTA partition.
//
// GitHub release naming contract:
//   Tag:   vX.Y.Z (the same format as FW_VERSION in version.txt)
//   Asset: hometiles_<tag>_<device-key>.bin
//   Example: hometiles_v0.3.2_waveshare_touch_lcd_8.bin
// releaseAssetDeviceKey() derives <device-key> from Device::profile().key
// and the validated silicon variant. Firmware up to v0.2.9 used the
// esp32-p4-homeassistant-display-* schema. install() retains that fallback for
// old devices, but new releases do not need to publish the legacy names.
//
// The repository was renamed from ESP32-P4-HomeAssistant-Display to HomeTiles.
// GitHub permanently redirects the old URL, so older firmware continues to
// work. Never recreate a repository at the old URL because that would break
// the redirect.
namespace GithubUpdate {

constexpr const char* kRepoUrl =
    "https://github.com/GalusPeres/HomeTiles";

struct CheckResult {
  bool ok = false;                // The request completed successfully.
  bool update_available = false;  // latest_tag is newer than FW_VERSION.
  bool tls_alloc_failed = false;  // TLS failed because internal RAM ran out.
  char latest_tag[24] = {};       // For example, "v0.3.0".
};

// Blocks for 1-3 seconds during the TLS handshake. Call only from the loop
// task, never directly from an LVGL event callback. Use the same pending-flag
// pattern as the hotspot toggle.
CheckResult checkLatest();

// Progress is measured in bytes; total remains zero while the size is
// unknown. The callback also drives the caller's LVGL pump cadence.
typedef void (*ProgressFn)(size_t written, size_t total);

// Downloads the active device profile's asset and writes it to the OTA
// partition. This can block for minutes. The caller first pauses MQTT and Web
// Admin because TLS needs about 45KB of internal RAM, and restarts the device
// after a true result. error_out contains the reason on false.
bool install(const char* tag, ProgressFn progress, String& error_out);

// Multiline diagnostics for the last install() run, including failed ranges,
// attempts, memory, and WiFi state. The caller appends this to the crash report
// before a safe restart because that restart does not create a core dump.
// Empty until install() has run.
String lastInstallDiag();

// True when the last install() failure happened after asset and firmware
// metadata validation, such as a transport interruption. Only then is an
// automatic retry after restart useful; retrying a device mismatch would only
// create a reboot loop.
bool lastInstallRetryable();

}  // namespace GithubUpdate

#endif  // GITHUB_UPDATE_H
