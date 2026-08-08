#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef HOMETILES_GUITION_S3_DIAGNOSTICS
#define HOMETILES_GUITION_S3_DIAGNOSTICS 0
#endif

#if defined(DEVICE_GUITION_ESP32_4848S040) && \
    HOMETILES_GUITION_S3_DIAGNOSTICS
#define HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE 1
#else
#define HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE 0
#endif

namespace GuitionS3Diagnostics {

enum class TouchPollIssue : uint8_t {
  NoNewFrame,
  StatusRead,
  PointRead,
  StatusWrite,
  InvalidPointCount,
  InvalidCoordinates,
};

enum class TouchAction : uint8_t {
  ShortClicked,
  Clicked,
  LongPressed,
};

#if HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE

void logBoot(const char* firmware_version, const char* device_profile);
void logMemory(const char* phase, void* loop_task, void* mqtt_task);
void logTaskWatermark(const char* task_name, void* task);
void logOtaPartitions(const char* phase);

void noteTouchDown(uint32_t timestamp_ms, int16_t x, int16_t y,
                   uint16_t raw_x, uint16_t raw_y, uint8_t status);
void noteTouchRelease(uint32_t timestamp_ms, int16_t x, int16_t y,
                      uint8_t status);
void noteTouchPollIssue(TouchPollIssue issue);
void noteTouchAction(TouchAction action, uint32_t timestamp_ms,
                     const void* target, int16_t x, int16_t y);

void noteUiLoop(uint32_t pre_lvgl_us, uint32_t lvgl_us);
void noteFlush(uint32_t pixels, uint32_t duration_us, bool is_last,
               int16_t x1, int16_t y1, int16_t x2, int16_t y2);

void logSlideshowDecode(const char* file_name, size_t file_bytes,
                        uint16_t source_w, uint16_t source_h,
                        uint16_t target_w, uint16_t target_h,
                        uint32_t stride_bytes, const char* decoder,
                        bool ok, uint32_t read_ms, uint32_t decode_ms,
                        uint32_t cover_ms);
void beginSlideshowPresentation(const char* file_name, bool cache_hit,
                                uint16_t width, uint16_t height,
                                uint32_t stride_bytes);

void noteStorageGuardBegin(bool blackout);
void noteStorageGuardEnd(uint32_t duration_ms);
void noteRgbRestart(int32_t esp_error, uint32_t recovery_ms);

// Drain event records and emit rate-limited summaries from the normal loop.
// The touch and flush callbacks only update fixed-size counters/records.
void service();

#else

inline void logBoot(const char*, const char*) {}
inline void logMemory(const char*, void*, void*) {}
inline void logTaskWatermark(const char*, void*) {}
inline void logOtaPartitions(const char*) {}
inline void noteTouchDown(uint32_t, int16_t, int16_t, uint16_t, uint16_t,
                          uint8_t) {}
inline void noteTouchRelease(uint32_t, int16_t, int16_t, uint8_t) {}
inline void noteTouchPollIssue(TouchPollIssue) {}
inline void noteTouchAction(TouchAction, uint32_t, const void*, int16_t,
                            int16_t) {}
inline void noteUiLoop(uint32_t, uint32_t) {}
inline void noteFlush(uint32_t, uint32_t, bool, int16_t, int16_t, int16_t,
                      int16_t) {}
inline void logSlideshowDecode(const char*, size_t, uint16_t, uint16_t,
                               uint16_t, uint16_t, uint32_t, const char*,
                               bool, uint32_t, uint32_t, uint32_t) {}
inline void beginSlideshowPresentation(const char*, bool, uint16_t, uint16_t,
                                       uint32_t) {}
inline void noteStorageGuardBegin(bool) {}
inline void noteStorageGuardEnd(uint32_t) {}
inline void noteRgbRestart(int32_t, uint32_t) {}
inline void service() {}

#endif

}  // namespace GuitionS3Diagnostics
