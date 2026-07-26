#include "src/video/camera_stream.h"

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "src/core/config_manager.h"
#include "src/core/i18n.h"

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8)

#include <HTTPClient.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <esp_h264_dec.h>
#include <esp_h264_dec_sw.h>

#include <algorithm>
#include <cstring>

#include "src/devices/waveshare_touch_lcd_8/device_waveshare_touch_lcd_8.h"

namespace {

constexpr uint16_t kWidth = 640;
constexpr uint16_t kHeight = 480;
constexpr size_t kPixelBytes =
    static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t);
constexpr size_t kI420Bytes =
    static_cast<size_t>(kWidth) * kHeight * 3U / 2U;
constexpr size_t kInputBytes = 192U * 1024U;
constexpr uint8_t kFrameBufferCount = 2;
constexpr size_t kReadChunkBytes = 32U * 1024U;

enum class FrameState : uint8_t {
  Free,
  Writing,
  Ready,
  Displayed,
};

portMUX_TYPE g_state_mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t g_task = nullptr;
volatile bool g_stop_requested = false;
bool g_release_buffers = false;
String g_url;
uint16_t* g_pixels[kFrameBufferCount] = {};
lv_image_dsc_t g_images[kFrameBufferCount] = {};
FrameState g_frame_states[kFrameBufferCount] = {};
int8_t g_displayed_index = -1;
uint32_t g_status_sequence = 0;
uint32_t g_ui_status_sequence = 0;
char g_status[96] = "";
bool g_status_error = false;
uint32_t g_worker_frame_count = 0;

// BT.601 limited-range lookup tables remove five multiplications per output
// pixel. Four pixels share one U/V pair, which keeps the software conversion
// from monopolising the P4 while LVGL is drawing the rest of the UI.
bool g_color_tables_ready = false;
int32_t g_y_term[256];
int32_t g_r_from_v[256];
int32_t g_g_from_u[256];
int32_t g_g_from_v[256];
int32_t g_b_from_u[256];

static const i18n::Strings& camera_text() {
  return i18n::strings(configManager.getConfig().language);
}

static uint8_t clamp_u8(int32_t value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

static void initialize_color_tables() {
  if (g_color_tables_ready) return;
  for (int value = 0; value < 256; ++value) {
    const int y = std::max(0, value - 16);
    const int chroma = value - 128;
    g_y_term[value] = 298 * y;
    g_r_from_v[value] = 409 * chroma;
    g_g_from_u[value] = -100 * chroma;
    g_g_from_v[value] = -208 * chroma;
    g_b_from_u[value] = 516 * chroma;
  }
  g_color_tables_ready = true;
}

static uint16_t rgb565_swapped(uint8_t y_value,
                               int32_t r_add,
                               int32_t g_add,
                               int32_t b_add) {
  const int32_t y = g_y_term[y_value];
  const uint8_t r = clamp_u8((y + r_add + 128) >> 8);
  const uint8_t g = clamp_u8((y + g_add + 128) >> 8);
  const uint8_t b = clamp_u8((y + b_add + 128) >> 8);
  const uint16_t rgb565 =
      static_cast<uint16_t>(((r & 0xF8U) << 8) |
                            ((g & 0xFCU) << 3) |
                            (b >> 3));
  return static_cast<uint16_t>((rgb565 >> 8) | (rgb565 << 8));
}

static void set_status(const char* text, bool error = false) {
  portENTER_CRITICAL(&g_state_mux);
  snprintf(g_status, sizeof(g_status), "%s", text ? text : "");
  g_status_error = error;
  ++g_status_sequence;
  portEXIT_CRITICAL(&g_state_mux);
}

static bool task_is_active() {
  portENTER_CRITICAL(&g_state_mux);
  const bool active = g_task != nullptr;
  portEXIT_CRITICAL(&g_state_mux);
  return active;
}

static void release_frame_buffers() {
  uint16_t* buffers[kFrameBufferCount] = {};
  portENTER_CRITICAL(&g_state_mux);
  for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
    buffers[i] = g_pixels[i];
    g_pixels[i] = nullptr;
    memset(&g_images[i], 0, sizeof(g_images[i]));
    g_frame_states[i] = FrameState::Free;
  }
  g_displayed_index = -1;
  portEXIT_CRITICAL(&g_state_mux);

  for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
    if (buffers[i]) heap_caps_free(buffers[i]);
  }
}

static bool ensure_frame_buffers() {
  for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
    if (g_pixels[i]) continue;
    g_pixels[i] = static_cast<uint16_t*>(heap_caps_aligned_alloc(
        64, kPixelBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_pixels[i]) {
      set_status(camera_text().camera_frame_memory_failed, true);
      release_frame_buffers();
      return false;
    }
    memset(g_pixels[i], 0, kPixelBytes);
    memset(&g_images[i], 0, sizeof(g_images[i]));
    g_images[i].header.magic = LV_IMAGE_HEADER_MAGIC;
    g_images[i].header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
    g_images[i].header.w = kWidth;
    g_images[i].header.h = kHeight;
    g_images[i].header.stride = kWidth * sizeof(uint16_t);
    g_images[i].data_size = kPixelBytes;
    g_images[i].data =
        reinterpret_cast<const uint8_t*>(g_pixels[i]);
    g_frame_states[i] = FrameState::Free;
  }
  return true;
}

static int8_t acquire_write_buffer() {
  int8_t selected = -1;
  portENTER_CRITICAL(&g_state_mux);
  for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
    if (g_frame_states[i] == FrameState::Free) {
      selected = static_cast<int8_t>(i);
      break;
    }
  }
  if (selected < 0) {
    // A frame not yet shown by LVGL is safe to replace. The buffer currently
    // displayed is never written by the decoder task.
    for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
      if (g_frame_states[i] == FrameState::Ready) {
        selected = static_cast<int8_t>(i);
        break;
      }
    }
  }
  if (selected >= 0) {
    g_frame_states[selected] = FrameState::Writing;
  }
  portEXIT_CRITICAL(&g_state_mux);
  return selected;
}

static void publish_write_buffer(int8_t index) {
  if (index < 0 || index >= static_cast<int8_t>(kFrameBufferCount)) return;
  portENTER_CRITICAL(&g_state_mux);
  g_frame_states[index] = FrameState::Ready;
  portEXIT_CRITICAL(&g_state_mux);
}

static void release_write_buffer(int8_t index) {
  if (index < 0 || index >= static_cast<int8_t>(kFrameBufferCount)) return;
  portENTER_CRITICAL(&g_state_mux);
  if (g_frame_states[index] == FrameState::Writing) {
    g_frame_states[index] = FrameState::Free;
  }
  portEXIT_CRITICAL(&g_state_mux);
}

static bool i420_to_rgb565_swapped(const uint8_t* src, uint16_t* dst) {
  initialize_color_tables();
  const uint8_t* y_plane = src;
  const uint8_t* u_plane = y_plane + static_cast<size_t>(kWidth) * kHeight;
  const uint8_t* v_plane =
      u_plane + static_cast<size_t>(kWidth / 2U) * (kHeight / 2U);

  for (uint16_t y = 0; y < kHeight; y += 2) {
    if ((y & 7U) == 0U) {
      if (g_stop_requested) return false;
      taskYIELD();
    }
    const size_t row0 = static_cast<size_t>(y) * kWidth;
    const size_t row1 = row0 + kWidth;
    const size_t uv_row = static_cast<size_t>(y / 2U) * (kWidth / 2U);
    for (uint16_t x = 0; x < kWidth; x += 2) {
      const uint8_t u = u_plane[uv_row + x / 2U];
      const uint8_t v = v_plane[uv_row + x / 2U];
      const int32_t r_add = g_r_from_v[v];
      const int32_t g_add = g_g_from_u[u] + g_g_from_v[v];
      const int32_t b_add = g_b_from_u[u];

      dst[row0 + x] =
          rgb565_swapped(y_plane[row0 + x], r_add, g_add, b_add);
      dst[row0 + x + 1] =
          rgb565_swapped(y_plane[row0 + x + 1], r_add, g_add, b_add);
      dst[row1 + x] =
          rgb565_swapped(y_plane[row1 + x], r_add, g_add, b_add);
      dst[row1 + x + 1] =
          rgb565_swapped(y_plane[row1 + x + 1], r_add, g_add, b_add);
    }
  }
  return !g_stop_requested;
}

static bool output_frame(const esp_h264_dec_out_frame_t& output) {
  if (!output.outbuf || output.out_size == 0) return true;
  if (output.out_size != kI420Bytes) {
    char message[96];
    snprintf(message, sizeof(message),
             camera_text().camera_resolution_error_fmt,
             static_cast<unsigned>(output.out_size));
    set_status(message, true);
    return false;
  }

  const int8_t index = acquire_write_buffer();
  if (index < 0) return true;
  if (!i420_to_rgb565_swapped(output.outbuf, g_pixels[index])) {
    release_write_buffer(index);
    return false;
  }
  publish_write_buffer(index);
  ++g_worker_frame_count;
  return true;
}

static bool decode_buffer(esp_h264_dec_handle_t decoder,
                          uint8_t* input,
                          size_t& buffered) {
  size_t offset = 0;
  while (offset < buffered && !g_stop_requested) {
    esp_h264_dec_in_frame_t in_frame{};
    in_frame.raw_data.buffer = input + offset;
    in_frame.raw_data.len =
        static_cast<uint32_t>(buffered - offset);
    esp_h264_dec_out_frame_t out_frame{};
    const esp_h264_err_t result =
        esp_h264_dec_process(decoder, &in_frame, &out_frame);
    if (result != ESP_H264_ERR_OK) {
      set_status(camera_text().camera_decoder_error, true);
      return false;
    }
    if (!output_frame(out_frame)) return false;
    if (in_frame.consume == 0) break;
    offset += in_frame.consume;
  }

  if (offset > 0) {
    const size_t remaining = buffered - offset;
    if (remaining) memmove(input, input + offset, remaining);
    buffered = remaining;
  }
  return true;
}

static void finish_camera_task(HTTPClient* http,
                               esp_h264_dec_handle_t decoder,
                               uint8_t* input) {
  if (input) heap_caps_free(input);
  if (decoder) {
    esp_h264_dec_close(decoder);
    esp_h264_dec_del(decoder);
  }
  if (http) http->end();

  bool release_buffers = false;
  portENTER_CRITICAL(&g_state_mux);
  g_task = nullptr;
  release_buffers = g_release_buffers;
  portEXIT_CRITICAL(&g_state_mux);
  if (release_buffers) release_frame_buffers();
  vTaskDelete(nullptr);
}

static void camera_task(void*) {
  set_status(camera_text().camera_http_connecting);
  const String url = g_url;
  NetworkClient plain_client;
  NetworkClientSecure secure_client;
  secure_client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(250);
  http.setReuse(false);

  const bool secure = url.startsWith("https://");
  const bool began = secure ? http.begin(secure_client, url)
                            : http.begin(plain_client, url);
  if (!began) {
    set_status(camera_text().camera_url_open_failed, true);
    finish_camera_task(&http, nullptr, nullptr);
    return;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char message[64];
    snprintf(message, sizeof(message), camera_text().camera_http_error_fmt,
             code);
    set_status(message, true);
    finish_camera_task(&http, nullptr, nullptr);
    return;
  }

  esp_h264_dec_cfg_sw_t decoder_cfg{};
  decoder_cfg.pic_type = ESP_H264_RAW_FMT_I420;
  esp_h264_dec_handle_t decoder = nullptr;
  if (esp_h264_dec_sw_new(&decoder_cfg, &decoder) != ESP_H264_ERR_OK ||
      !decoder ||
      esp_h264_dec_open(decoder) != ESP_H264_ERR_OK) {
    set_status(camera_text().camera_decoder_start_failed, true);
    if (decoder) esp_h264_dec_del(decoder);
    finish_camera_task(&http, nullptr, nullptr);
    return;
  }

  uint8_t* input = static_cast<uint8_t*>(heap_caps_malloc(
      kInputBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input) {
    set_status(camera_text().camera_input_memory_failed, true);
    finish_camera_task(&http, decoder, nullptr);
    return;
  }

  NetworkClient* stream = http.getStreamPtr();
  size_t buffered = 0;
  uint32_t frame_count = 0;
  uint32_t last_fps_ms = millis();
  set_status(camera_text().camera_buffering);

  while (!g_stop_requested && http.connected()) {
    const int available = stream ? stream->available() : 0;
    if (available <= 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    const size_t room = kInputBytes - buffered;
    if (room == 0) {
      set_status(camera_text().camera_input_buffer_full, true);
      break;
    }
    const size_t wanted =
        std::min(std::min(room, static_cast<size_t>(available)),
                 kReadChunkBytes);
    const int received = stream->read(input + buffered, wanted);
    if (received <= 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    buffered += static_cast<size_t>(received);

    const uint32_t frames_before = g_worker_frame_count;
    if (!decode_buffer(decoder, input, buffered)) break;
    frame_count += g_worker_frame_count - frames_before;

    const uint32_t now = millis();
    if (now - last_fps_ms >= 2000) {
      const float fps =
          static_cast<float>(frame_count) * 1000.0f /
          static_cast<float>(now - last_fps_ms);
      char message[64];
      snprintf(message, sizeof(message), camera_text().camera_fps_fmt,
               fps);
      set_status(message);
      frame_count = 0;
      last_fps_ms = now;
    }
    taskYIELD();
  }

  if (g_stop_requested) {
    set_status(camera_text().camera_stream_stopped);
  } else {
    bool had_error = false;
    portENTER_CRITICAL(&g_state_mux);
    had_error = g_status_error;
    portEXIT_CRITICAL(&g_state_mux);
    if (!had_error) set_status(camera_text().camera_connection_ended, true);
  }
  finish_camera_task(&http, decoder, input);
}

}  // namespace

bool camera_stream_start(const char* url) {
  if (!url || !*url) {
    set_status(camera_text().camera_empty_url, true);
    return false;
  }
  if (task_is_active()) {
    set_status(camera_text().camera_already_running, true);
    return false;
  }
  if (!ensure_frame_buffers()) return false;

  portENTER_CRITICAL(&g_state_mux);
  for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
    g_frame_states[i] =
        (static_cast<int8_t>(i) == g_displayed_index)
            ? FrameState::Displayed
            : FrameState::Free;
  }
  g_release_buffers = false;
  portEXIT_CRITICAL(&g_state_mux);

  g_url = url;
  g_stop_requested = false;
  g_worker_frame_count = 0;
  const BaseType_t task_core = (ARDUINO_RUNNING_CORE == 0) ? 1 : 0;
  if (xTaskCreatePinnedToCoreWithCaps(
          camera_task, "cameraH264", 16384, nullptr, tskIDLE_PRIORITY,
          &g_task, task_core, MALLOC_CAP_SPIRAM) != pdPASS) {
    portENTER_CRITICAL(&g_state_mux);
    g_task = nullptr;
    g_release_buffers = true;
    portEXIT_CRITICAL(&g_state_mux);
    release_frame_buffers();
    set_status(camera_text().camera_task_start_failed, true);
    return false;
  }
  return true;
}

void camera_stream_stop() {
  g_stop_requested = true;
  bool active = false;
  portENTER_CRITICAL(&g_state_mux);
  g_release_buffers = true;
  active = g_task != nullptr;
  portEXIT_CRITICAL(&g_state_mux);
  if (!active) release_frame_buffers();
}

void camera_stream_process_ui(lv_obj_t* image,
                              lv_obj_t* placeholder,
                              lv_obj_t* status_label) {
  if (!image) return;
  int8_t ready = -1;
  // If the display PPA has entered its short self-healing cooldown, keep the
  // last frame. Repainting 640x480 through the CPU fallback would make every
  // other LVGL interaction sluggish and would fight the driver's recovery.
  if (!DeviceWaveshareTouchLCD8::ppaCooldownActive()) {
    portENTER_CRITICAL(&g_state_mux);
    for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
      if (g_frame_states[i] == FrameState::Ready) {
        ready = static_cast<int8_t>(i);
      }
    }
    if (ready >= 0) {
      if (g_displayed_index >= 0 &&
          g_displayed_index < static_cast<int8_t>(kFrameBufferCount) &&
          g_frame_states[g_displayed_index] == FrameState::Displayed) {
        g_frame_states[g_displayed_index] = FrameState::Free;
      }
      for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
        if (static_cast<int8_t>(i) != ready &&
            g_frame_states[i] == FrameState::Ready) {
          g_frame_states[i] = FrameState::Free;
        }
      }
      g_frame_states[ready] = FrameState::Displayed;
      g_displayed_index = ready;
    }
    portEXIT_CRITICAL(&g_state_mux);
  }

  if (ready >= 0) {
    lv_image_set_src(image, &g_images[ready]);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);
    if (placeholder) lv_obj_add_flag(placeholder, LV_OBJ_FLAG_HIDDEN);
  }

  char status[sizeof(g_status)] = "";
  bool status_error = false;
  bool status_changed = false;
  portENTER_CRITICAL(&g_state_mux);
  if (g_status_sequence != g_ui_status_sequence) {
    snprintf(status, sizeof(status), "%s", g_status);
    status_error = g_status_error;
    g_ui_status_sequence = g_status_sequence;
    status_changed = true;
  }
  portEXIT_CRITICAL(&g_state_mux);
  if (status_changed && status_label) {
    lv_label_set_text(status_label, status);
    lv_obj_set_style_text_color(
        status_label,
        status_error ? lv_color_hex(0xFF6B6B) : lv_color_hex(0xD8DEE9), 0);
  }
}

void camera_stream_set_external_status(const char* text, bool error) {
  set_status(text, error);
}

bool camera_stream_is_active() {
  return task_is_active();
}

#else

bool camera_stream_start(const char*) {
  camera_stream_set_external_status(
      i18n::strings(configManager.getConfig().language).camera_device_only,
      true);
  return false;
}

void camera_stream_stop() {}
void camera_stream_process_ui(lv_obj_t*, lv_obj_t*, lv_obj_t*) {}
void camera_stream_set_external_status(const char*, bool) {}
bool camera_stream_is_active() { return false; }

#endif
