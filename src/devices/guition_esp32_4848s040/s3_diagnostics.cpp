#include "src/devices/guition_esp32_4848s040/s3_diagnostics.h"

#if HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE

#include <Arduino.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>

namespace GuitionS3Diagnostics {
namespace {

constexpr uint32_t kSummaryIntervalMs = 15000;
constexpr size_t kTouchEventCapacity = 12;

enum class TouchEventKind : uint8_t {
  Down,
  Release,
  ShortClicked,
  Clicked,
  LongPressed,
};

struct TouchEventRecord {
  TouchEventKind kind = TouchEventKind::Down;
  uint32_t timestamp_ms = 0;
  int16_t x = 0;
  int16_t y = 0;
  uint16_t raw_x = 0;
  uint16_t raw_y = 0;
  uint8_t status = 0;
  const void* target = nullptr;
  uint32_t down_age_ms = 0;
};

struct UiStats {
  uint32_t interval_started_ms = 0;
  uint32_t loop_count = 0;
  uint64_t pre_lvgl_us = 0;
  uint32_t pre_lvgl_max_us = 0;
  uint64_t lvgl_us = 0;
  uint32_t lvgl_max_us = 0;
  uint32_t lvgl_over_20ms = 0;
  uint32_t lvgl_over_50ms = 0;
  uint32_t flush_count = 0;
  uint32_t frame_count = 0;
  uint64_t flush_pixels = 0;
  uint64_t flush_us = 0;
  uint32_t flush_max_us = 0;
};

struct SlidePresentation {
  bool active = false;
  bool completed = false;
  uint32_t seq = 0;
  uint32_t started_ms = 0;
  uint32_t completed_ms = 0;
  uint32_t resync_before = 0;
  uint32_t resync_after = 0;
  uint32_t flush_count = 0;
  uint64_t pixels = 0;
  int16_t x1 = 0;
  int16_t y1 = 0;
  int16_t x2 = 0;
  int16_t y2 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t stride = 0;
  bool cache_hit = false;
  char file_name[72] = {};
};

struct StorageStats {
  uint32_t interval_started_ms = 0;
  uint32_t guard_started_ms = 0;
  uint32_t guards = 0;
  uint32_t blackouts = 0;
  uint32_t restart_ok = 0;
  uint32_t restart_failed = 0;
  uint32_t total_ms = 0;
  uint32_t max_ms = 0;
  int32_t last_restart_error = 0;
  uint32_t last_recovery_ms = 0;
};

TouchEventRecord g_touch_events[kTouchEventCapacity];
volatile uint8_t g_touch_event_head = 0;
volatile uint8_t g_touch_event_tail = 0;
volatile uint32_t g_touch_event_dropped = 0;
volatile uint32_t g_touch_no_new_frame = 0;
volatile uint32_t g_touch_status_read_errors = 0;
volatile uint32_t g_touch_point_read_errors = 0;
volatile uint32_t g_touch_status_write_errors = 0;
volatile uint32_t g_touch_invalid_count = 0;
volatile uint32_t g_touch_invalid_coordinates = 0;
uint32_t g_touch_down_ms = 0;
int16_t g_touch_last_x = 0;
int16_t g_touch_last_y = 0;
uint32_t g_touch_summary_ms = 0;

UiStats g_ui;
SlidePresentation g_slide;
StorageStats g_storage;
uint32_t g_rgb_resync_count = 0;

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-wdt";
    case ESP_RST_TASK_WDT: return "task-wdt";
    case ESP_RST_WDT: return "other-wdt";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    case ESP_RST_USB: return "usb";
    case ESP_RST_JTAG: return "jtag";
    case ESP_RST_EFUSE: return "efuse";
    case ESP_RST_PWR_GLITCH: return "power-glitch";
    case ESP_RST_CPU_LOCKUP: return "cpu-lockup";
    default: return "unknown";
  }
}

const char* otaStateName(esp_ota_img_states_t state) {
  switch (state) {
    case ESP_OTA_IMG_NEW: return "new";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending-verify";
    case ESP_OTA_IMG_VALID: return "valid";
    case ESP_OTA_IMG_INVALID: return "invalid";
    case ESP_OTA_IMG_ABORTED: return "aborted";
    case ESP_OTA_IMG_UNDEFINED: return "undefined";
    default: return "unknown";
  }
}

void logPartition(const char* phase, const char* role,
                  const esp_partition_t* partition) {
  if (!partition) {
    Serial.printf("[S3Diag/OTA] phase=%s role=%s result=missing\n",
                  phase ? phase : "?", role ? role : "?");
    return;
  }

  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  const esp_err_t state_err =
      partition->type == ESP_PARTITION_TYPE_APP
          ? esp_ota_get_state_partition(partition, &state)
          : ESP_ERR_NOT_SUPPORTED;
  Serial.printf(
      "[S3Diag/OTA] phase=%s role=%s label=%s type=0x%02X "
      "subtype=0x%02X addr=0x%08lX size=0x%08lX encrypted=%u "
      "state=%s(%d) state_err=%s(0x%lX)\n",
      phase ? phase : "?", role ? role : "?", partition->label,
      static_cast<unsigned>(partition->type),
      static_cast<unsigned>(partition->subtype),
      static_cast<unsigned long>(partition->address),
      static_cast<unsigned long>(partition->size),
      partition->encrypted ? 1U : 0U,
      state_err == ESP_OK ? otaStateName(state) : "n/a",
      state_err == ESP_OK ? static_cast<int>(state) : -1,
      esp_err_to_name(state_err), static_cast<unsigned long>(state_err));
}

void queueTouchEvent(const TouchEventRecord& event) {
  const uint8_t head = g_touch_event_head;
  const uint8_t next = static_cast<uint8_t>((head + 1U) % kTouchEventCapacity);
  if (next == g_touch_event_tail) {
    ++g_touch_event_dropped;
    return;
  }
  g_touch_events[head] = event;
  g_touch_event_head = next;
}

const char* touchEventName(TouchEventKind kind) {
  switch (kind) {
    case TouchEventKind::Down: return "DOWN";
    case TouchEventKind::Release: return "RELEASE";
    case TouchEventKind::ShortClicked: return "SHORT_CLICKED";
    case TouchEventKind::Clicked: return "CLICKED";
    case TouchEventKind::LongPressed: return "LONG_PRESSED";
    default: return "?";
  }
}

void drainTouchEvents() {
  while (g_touch_event_tail != g_touch_event_head) {
    const uint8_t tail = g_touch_event_tail;
    const TouchEventRecord event = g_touch_events[tail];
    g_touch_event_tail =
        static_cast<uint8_t>((tail + 1U) % kTouchEventCapacity);
    if (event.kind == TouchEventKind::Down) {
      Serial.printf(
          "[S3Diag/Touch] state=DOWN t_ms=%lu x=%d y=%d raw_x=%u "
          "raw_y=%u status=0x%02X\n",
          static_cast<unsigned long>(event.timestamp_ms), event.x, event.y,
          static_cast<unsigned>(event.raw_x),
          static_cast<unsigned>(event.raw_y),
          static_cast<unsigned>(event.status));
    } else if (event.kind == TouchEventKind::Release) {
      Serial.printf(
          "[S3Diag/Touch] state=RELEASE t_ms=%lu held_ms=%lu last_x=%d "
          "last_y=%d status=0x%02X\n",
          static_cast<unsigned long>(event.timestamp_ms),
          static_cast<unsigned long>(event.down_age_ms), event.x, event.y,
          static_cast<unsigned>(event.status));
    } else {
      Serial.printf(
          "[S3Diag/Touch] action=%s t_ms=%lu target=%p x=%d y=%d "
          "down_age_ms=%lu\n",
          touchEventName(event.kind),
          static_cast<unsigned long>(event.timestamp_ms), event.target,
          event.x, event.y,
          static_cast<unsigned long>(event.down_age_ms));
    }
  }
}

void maybeLogTouchSummary(uint32_t now) {
  if (g_touch_summary_ms == 0) g_touch_summary_ms = now;
  if (now - g_touch_summary_ms < kSummaryIntervalMs) return;
  g_touch_summary_ms = now;
  const uint32_t no_frame = g_touch_no_new_frame;
  const uint32_t status_read = g_touch_status_read_errors;
  const uint32_t point_read = g_touch_point_read_errors;
  const uint32_t status_write = g_touch_status_write_errors;
  const uint32_t invalid_count = g_touch_invalid_count;
  const uint32_t invalid_xy = g_touch_invalid_coordinates;
  const uint32_t dropped = g_touch_event_dropped;
  g_touch_no_new_frame = 0;
  g_touch_status_read_errors = 0;
  g_touch_point_read_errors = 0;
  g_touch_status_write_errors = 0;
  g_touch_invalid_count = 0;
  g_touch_invalid_coordinates = 0;
  g_touch_event_dropped = 0;
  if (status_read || point_read || status_write || invalid_count || invalid_xy ||
      dropped) {
    Serial.printf(
        "[S3Diag/Touch] interval_ms=%lu no_new_frame=%lu "
        "status_read_err=%lu point_read_err=%lu status_write_err=%lu "
        "invalid_count=%lu invalid_xy=%lu dropped=%lu\n",
        static_cast<unsigned long>(kSummaryIntervalMs),
        static_cast<unsigned long>(no_frame),
        static_cast<unsigned long>(status_read),
        static_cast<unsigned long>(point_read),
        static_cast<unsigned long>(status_write),
        static_cast<unsigned long>(invalid_count),
        static_cast<unsigned long>(invalid_xy),
        static_cast<unsigned long>(dropped));
  }
}

void maybeLogUiSummary(uint32_t now) {
  if (g_ui.interval_started_ms == 0) g_ui.interval_started_ms = now;
  if (now - g_ui.interval_started_ms < kSummaryIntervalMs) return;
  const uint32_t interval = now - g_ui.interval_started_ms;
  if (g_ui.loop_count || g_ui.flush_count) {
    Serial.printf(
        "[S3Diag/UI] interval_ms=%lu loops=%lu pre_avg_us=%llu "
        "pre_max_us=%lu lvgl_avg_us=%llu lvgl_max_us=%lu "
        "lvgl_gt20ms=%lu lvgl_gt50ms=%lu frames=%lu flushes=%lu "
        "flush_px=%llu flush_avg_us=%llu flush_max_us=%lu\n",
        static_cast<unsigned long>(interval),
        static_cast<unsigned long>(g_ui.loop_count),
        static_cast<unsigned long long>(
            g_ui.loop_count ? g_ui.pre_lvgl_us / g_ui.loop_count : 0),
        static_cast<unsigned long>(g_ui.pre_lvgl_max_us),
        static_cast<unsigned long long>(
            g_ui.loop_count ? g_ui.lvgl_us / g_ui.loop_count : 0),
        static_cast<unsigned long>(g_ui.lvgl_max_us),
        static_cast<unsigned long>(g_ui.lvgl_over_20ms),
        static_cast<unsigned long>(g_ui.lvgl_over_50ms),
        static_cast<unsigned long>(g_ui.frame_count),
        static_cast<unsigned long>(g_ui.flush_count),
        static_cast<unsigned long long>(g_ui.flush_pixels),
        static_cast<unsigned long long>(
            g_ui.flush_count ? g_ui.flush_us / g_ui.flush_count : 0),
        static_cast<unsigned long>(g_ui.flush_max_us));
  }
  g_ui = UiStats{};
  g_ui.interval_started_ms = now;
}

void maybeLogSlide(uint32_t now) {
  if (g_slide.completed) {
    Serial.printf(
        "[S3Diag/Slide] seq=%lu present=ok file=%s cache=%u output=%ux%u "
        "stride=%lu flushes=%lu pixels=%llu bounds=%d,%d..%d,%d "
        "presentation_ms=%lu resync_delta=%lu\n",
        static_cast<unsigned long>(g_slide.seq), g_slide.file_name,
        g_slide.cache_hit ? 1U : 0U,
        static_cast<unsigned>(g_slide.width),
        static_cast<unsigned>(g_slide.height),
        static_cast<unsigned long>(g_slide.stride),
        static_cast<unsigned long>(g_slide.flush_count),
        static_cast<unsigned long long>(g_slide.pixels), g_slide.x1,
        g_slide.y1, g_slide.x2, g_slide.y2,
        static_cast<unsigned long>(g_slide.completed_ms - g_slide.started_ms),
        static_cast<unsigned long>(g_slide.resync_after -
                                   g_slide.resync_before));
    g_slide.completed = false;
  } else if (g_slide.active && now - g_slide.started_ms >= 3000U) {
    Serial.printf(
        "[S3Diag/Slide] seq=%lu present=timeout file=%s elapsed_ms=%lu "
        "flushes=%lu pixels=%llu resync_delta=%lu\n",
        static_cast<unsigned long>(g_slide.seq), g_slide.file_name,
        static_cast<unsigned long>(now - g_slide.started_ms),
        static_cast<unsigned long>(g_slide.flush_count),
        static_cast<unsigned long long>(g_slide.pixels),
        static_cast<unsigned long>(g_rgb_resync_count -
                                   g_slide.resync_before));
    g_slide.active = false;
  }
}

void maybeLogStorage(uint32_t now) {
  if (g_storage.interval_started_ms == 0) g_storage.interval_started_ms = now;
  const bool first_event = g_storage.guards == 1 &&
                           g_storage.interval_started_ms == now;
  if (!first_event &&
      now - g_storage.interval_started_ms < kSummaryIntervalMs) {
    return;
  }
  if (g_storage.guards || g_storage.restart_ok || g_storage.restart_failed) {
    Serial.printf(
        "[S3Diag/Display] storage_guard interval_ms=%lu guards=%lu "
        "blackouts=%lu total_ms=%lu max_ms=%lu rgb_restart_ok=%lu "
        "rgb_restart_failed=%lu last_err=%s(0x%lX) recovery_ms=%lu "
        "resync_total=%lu\n",
        static_cast<unsigned long>(now - g_storage.interval_started_ms),
        static_cast<unsigned long>(g_storage.guards),
        static_cast<unsigned long>(g_storage.blackouts),
        static_cast<unsigned long>(g_storage.total_ms),
        static_cast<unsigned long>(g_storage.max_ms),
        static_cast<unsigned long>(g_storage.restart_ok),
        static_cast<unsigned long>(g_storage.restart_failed),
        esp_err_to_name(static_cast<esp_err_t>(g_storage.last_restart_error)),
        static_cast<unsigned long>(g_storage.last_restart_error),
        static_cast<unsigned long>(g_storage.last_recovery_ms),
        static_cast<unsigned long>(g_rgb_resync_count));
  }
  g_storage = StorageStats{};
  g_storage.interval_started_ms = now;
}

}  // namespace

void logBoot(const char* firmware_version, const char* device_profile) {
  const esp_reset_reason_t reset_reason = esp_reset_reason();
  Serial.printf(
      "[S3Diag/Boot] diagnostics=1 firmware=hometiles-%s profile=%s "
      "reset=%s(%d) flash=%lu psram=%lu\n",
      firmware_version ? firmware_version : "?",
      device_profile ? device_profile : "?", resetReasonName(reset_reason),
      static_cast<int>(reset_reason),
      static_cast<unsigned long>(ESP.getFlashChipSize()),
      static_cast<unsigned long>(ESP.getPsramSize()));
}

void logMemory(const char* phase, void* loop_task, void* mqtt_task) {
  TaskHandle_t loop_handle = static_cast<TaskHandle_t>(loop_task);
  TaskHandle_t mqtt_handle = static_cast<TaskHandle_t>(mqtt_task);
  if (!loop_handle) loop_handle = xTaskGetCurrentTaskHandle();
  Serial.printf(
      "[S3Diag/Mem] phase=%s int_free=%lu int_largest=%lu psram_free=%lu "
      "psram_largest=%lu loop_stack_hwm=%lu mqtt_stack_hwm=%ld\n",
      phase ? phase : "?",
      static_cast<unsigned long>(
          heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      static_cast<unsigned long>(heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      static_cast<unsigned long>(
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
      static_cast<unsigned long>(heap_caps_get_largest_free_block(
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
      static_cast<unsigned long>(uxTaskGetStackHighWaterMark(loop_handle)),
      mqtt_handle
          ? static_cast<long>(uxTaskGetStackHighWaterMark(mqtt_handle))
          : -1L);
}

void logTaskWatermark(const char* task_name, void* task) {
  TaskHandle_t handle = static_cast<TaskHandle_t>(task);
  if (!handle) handle = xTaskGetCurrentTaskHandle();
  Serial.printf("[S3Diag/Mem] task=%s stack_hwm=%lu\n",
                task_name ? task_name : "?",
                static_cast<unsigned long>(uxTaskGetStackHighWaterMark(handle)));
}

void logOtaPartitions(const char* phase) {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  const esp_partition_t* otadata = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  logPartition(phase, "running", running);
  logPartition(phase, "boot", boot);
  logPartition(phase, "next", next);
  logPartition(phase, "otadata", otadata);
}

void noteTouchDown(uint32_t timestamp_ms, int16_t x, int16_t y,
                   uint16_t raw_x, uint16_t raw_y, uint8_t status) {
  g_touch_down_ms = timestamp_ms;
  g_touch_last_x = x;
  g_touch_last_y = y;
  TouchEventRecord event;
  event.kind = TouchEventKind::Down;
  event.timestamp_ms = timestamp_ms;
  event.x = x;
  event.y = y;
  event.raw_x = raw_x;
  event.raw_y = raw_y;
  event.status = status;
  queueTouchEvent(event);
}

void noteTouchRelease(uint32_t timestamp_ms, int16_t x, int16_t y,
                      uint8_t status) {
  g_touch_last_x = x;
  g_touch_last_y = y;
  TouchEventRecord event;
  event.kind = TouchEventKind::Release;
  event.timestamp_ms = timestamp_ms;
  event.x = x;
  event.y = y;
  event.status = status;
  event.down_age_ms = timestamp_ms - g_touch_down_ms;
  queueTouchEvent(event);
}

void noteTouchPollIssue(TouchPollIssue issue) {
  switch (issue) {
    case TouchPollIssue::NoNewFrame: ++g_touch_no_new_frame; break;
    case TouchPollIssue::StatusRead: ++g_touch_status_read_errors; break;
    case TouchPollIssue::PointRead: ++g_touch_point_read_errors; break;
    case TouchPollIssue::StatusWrite: ++g_touch_status_write_errors; break;
    case TouchPollIssue::InvalidPointCount: ++g_touch_invalid_count; break;
    case TouchPollIssue::InvalidCoordinates:
      ++g_touch_invalid_coordinates;
      break;
  }
}

void noteTouchAction(TouchAction action, uint32_t timestamp_ms,
                     const void* target, int16_t x, int16_t y) {
  TouchEventRecord event;
  switch (action) {
    case TouchAction::ShortClicked:
      event.kind = TouchEventKind::ShortClicked;
      break;
    case TouchAction::Clicked: event.kind = TouchEventKind::Clicked; break;
    case TouchAction::LongPressed:
      event.kind = TouchEventKind::LongPressed;
      break;
  }
  event.timestamp_ms = timestamp_ms;
  event.target = target;
  event.x = x;
  event.y = y;
  event.down_age_ms = timestamp_ms - g_touch_down_ms;
  queueTouchEvent(event);
}

void noteUiLoop(uint32_t pre_lvgl_us, uint32_t lvgl_us) {
  ++g_ui.loop_count;
  g_ui.pre_lvgl_us += pre_lvgl_us;
  g_ui.pre_lvgl_max_us = std::max(g_ui.pre_lvgl_max_us, pre_lvgl_us);
  g_ui.lvgl_us += lvgl_us;
  g_ui.lvgl_max_us = std::max(g_ui.lvgl_max_us, lvgl_us);
  if (lvgl_us >= 20000U) ++g_ui.lvgl_over_20ms;
  if (lvgl_us >= 50000U) ++g_ui.lvgl_over_50ms;
}

void noteFlush(uint32_t pixels, uint32_t duration_us, bool is_last,
               int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  ++g_ui.flush_count;
  g_ui.flush_pixels += pixels;
  g_ui.flush_us += duration_us;
  g_ui.flush_max_us = std::max(g_ui.flush_max_us, duration_us);
  if (is_last) ++g_ui.frame_count;

  if (!g_slide.active) return;
  if (g_slide.flush_count == 0) {
    g_slide.x1 = x1;
    g_slide.y1 = y1;
    g_slide.x2 = x2;
    g_slide.y2 = y2;
  } else {
    g_slide.x1 = std::min(g_slide.x1, x1);
    g_slide.y1 = std::min(g_slide.y1, y1);
    g_slide.x2 = std::max(g_slide.x2, x2);
    g_slide.y2 = std::max(g_slide.y2, y2);
  }
  ++g_slide.flush_count;
  g_slide.pixels += pixels;
  if (is_last) {
    g_slide.completed_ms = millis();
    g_slide.resync_after = g_rgb_resync_count;
    g_slide.active = false;
    g_slide.completed = true;
  }
}

void logSlideshowDecode(const char* file_name, size_t file_bytes,
                        uint16_t source_w, uint16_t source_h,
                        uint16_t target_w, uint16_t target_h,
                        uint32_t stride_bytes, const char* decoder,
                        bool ok, uint32_t read_ms, uint32_t decode_ms,
                        uint32_t cover_ms) {
  Serial.printf(
      "[S3Diag/Slide] decode=%s file=%s file_bytes=%u decoder=%s "
      "source=%ux%u output=%ux%u cf=RGB565_SWAPPED stride=%lu "
      "data_bytes=%lu read_ms=%lu decode_ms=%lu cover_ms=%lu\n",
      ok ? "ok" : "failed", file_name ? file_name : "?",
      static_cast<unsigned>(file_bytes), decoder ? decoder : "none",
      static_cast<unsigned>(source_w), static_cast<unsigned>(source_h),
      static_cast<unsigned>(target_w), static_cast<unsigned>(target_h),
      static_cast<unsigned long>(stride_bytes),
      static_cast<unsigned long>(target_w) * target_h * sizeof(uint16_t),
      static_cast<unsigned long>(read_ms),
      static_cast<unsigned long>(decode_ms),
      static_cast<unsigned long>(cover_ms));
}

void beginSlideshowPresentation(const char* file_name, bool cache_hit,
                                uint16_t width, uint16_t height,
                                uint32_t stride_bytes) {
  const uint32_t next_seq = g_slide.seq + 1U;
  g_slide = SlidePresentation{};
  g_slide.active = true;
  g_slide.seq = next_seq;
  g_slide.started_ms = millis();
  g_slide.resync_before = g_rgb_resync_count;
  g_slide.width = width;
  g_slide.height = height;
  g_slide.stride = stride_bytes;
  g_slide.cache_hit = cache_hit;
  snprintf(g_slide.file_name, sizeof(g_slide.file_name), "%s",
           file_name ? file_name : "?");
  Serial.printf(
      "[S3Diag/Slide] seq=%lu present=queued file=%s cache=%u "
      "output=%ux%u stride=%lu resync_before=%lu\n",
      static_cast<unsigned long>(g_slide.seq), g_slide.file_name,
      cache_hit ? 1U : 0U, static_cast<unsigned>(width),
      static_cast<unsigned>(height), static_cast<unsigned long>(stride_bytes),
      static_cast<unsigned long>(g_slide.resync_before));
}

void noteStorageGuardBegin(bool blackout) {
  if (g_storage.guards == 0 && g_storage.restart_ok == 0 &&
      g_storage.restart_failed == 0) {
    g_storage.interval_started_ms = millis();
  }
  ++g_storage.guards;
  if (blackout) ++g_storage.blackouts;
  g_storage.guard_started_ms = millis();
}

void noteStorageGuardEnd(uint32_t duration_ms) {
  g_storage.total_ms += duration_ms;
  g_storage.max_ms = std::max(g_storage.max_ms, duration_ms);
  g_storage.guard_started_ms = 0;
}

void noteRgbRestart(int32_t esp_error, uint32_t recovery_ms) {
  g_storage.last_restart_error = esp_error;
  g_storage.last_recovery_ms = recovery_ms;
  if (esp_error == ESP_OK) {
    // esp_lcd_rgb_panel_restart() schedules the actual restart for the next
    // VSYNC on this SDK. Count only accepted manual requests; the IDF does not
    // expose a counter for its own automatic underflow recovery.
    ++g_rgb_resync_count;
    ++g_storage.restart_ok;
  } else {
    ++g_storage.restart_failed;
  }
}

void service() {
  const uint32_t now = millis();
  drainTouchEvents();
  maybeLogTouchSummary(now);
  maybeLogUiSummary(now);
  maybeLogSlide(now);
  maybeLogStorage(now);
}

}  // namespace GuitionS3Diagnostics

#endif  // HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE
