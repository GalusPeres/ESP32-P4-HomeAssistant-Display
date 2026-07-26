#include "src/video/camera_stream.h"

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8)

#include <HTTPClient.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <esp_h264_dec.h>
#include <esp_h264_dec_sw.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr uint16_t kWidth = 640;
constexpr uint16_t kHeight = 480;
constexpr size_t kPixelBytes =
    static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t);
constexpr size_t kI420Bytes =
    static_cast<size_t>(kWidth) * kHeight * 3U / 2U;
constexpr size_t kInputBytes = 256U * 1024U;
constexpr uint8_t kFrameBufferCount = 3;

enum class FrameState : uint8_t {
  Free,
  Writing,
  Ready,
  Displayed,
};

portMUX_TYPE g_state_mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t g_task = nullptr;
volatile bool g_stop_requested = false;
String g_url;
uint16_t* g_pixels[kFrameBufferCount] = {};
lv_image_dsc_t g_images[kFrameBufferCount] = {};
FrameState g_frame_states[kFrameBufferCount] = {};
int8_t g_displayed_index = -1;
uint32_t g_status_sequence = 0;
uint32_t g_ui_status_sequence = 0;
char g_status[96] = "Bereit";
bool g_status_error = false;
uint32_t g_worker_frame_count = 0;

static uint8_t clamp_u8(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

static void set_status(const char* text, bool error = false) {
  portENTER_CRITICAL(&g_state_mux);
  snprintf(g_status, sizeof(g_status), "%s", text ? text : "");
  g_status_error = error;
  ++g_status_sequence;
  portEXIT_CRITICAL(&g_state_mux);
}

static bool ensure_frame_buffers() {
  for (uint8_t i = 0; i < kFrameBufferCount; ++i) {
    if (!g_pixels[i]) {
      g_pixels[i] = static_cast<uint16_t*>(heap_caps_aligned_alloc(
          64, kPixelBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
      if (!g_pixels[i]) {
        set_status("Zu wenig PSRAM fuer Videobild", true);
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

static void i420_to_rgb565_swapped(const uint8_t* src, uint16_t* dst) {
  const uint8_t* y_plane = src;
  const uint8_t* u_plane = y_plane + static_cast<size_t>(kWidth) * kHeight;
  const uint8_t* v_plane =
      u_plane + static_cast<size_t>(kWidth / 2U) * (kHeight / 2U);

  for (uint16_t y = 0; y < kHeight; y += 2) {
    const size_t y_row0 = static_cast<size_t>(y) * kWidth;
    const size_t y_row1 = y_row0 + kWidth;
    const size_t uv_row = static_cast<size_t>(y / 2U) * (kWidth / 2U);
    for (uint16_t x = 0; x < kWidth; x += 2) {
      const int d = static_cast<int>(u_plane[uv_row + x / 2U]) - 128;
      const int e = static_cast<int>(v_plane[uv_row + x / 2U]) - 128;
      const int r_add = 409 * e + 128;
      const int g_add = -100 * d - 208 * e + 128;
      const int b_add = 516 * d + 128;

      for (uint8_t dy = 0; dy < 2; ++dy) {
        const size_t row = dy == 0 ? y_row0 : y_row1;
        for (uint8_t dx = 0; dx < 2; ++dx) {
          int c = static_cast<int>(y_plane[row + x + dx]) - 16;
          if (c < 0) c = 0;
          const uint8_t r = clamp_u8((298 * c + r_add) >> 8);
          const uint8_t g = clamp_u8((298 * c + g_add) >> 8);
          const uint8_t b = clamp_u8((298 * c + b_add) >> 8);
          const uint16_t rgb565 =
              static_cast<uint16_t>(((r & 0xF8U) << 8) |
                                    ((g & 0xFCU) << 3) |
                                    (b >> 3));
          dst[row + x + dx] =
              static_cast<uint16_t>((rgb565 >> 8) | (rgb565 << 8));
        }
      }
    }
    if ((y & 31U) == 0U) taskYIELD();
  }
}

static bool output_frame(const esp_h264_dec_out_frame_t& output) {
  if (!output.outbuf || output.out_size == 0) return true;
  if (output.out_size != kI420Bytes) {
    char message[96];
    snprintf(message, sizeof(message),
             "Unerwartete Videoaufloesung (%u Bytes)",
             static_cast<unsigned>(output.out_size));
    set_status(message, true);
    return false;
  }

  const int8_t index = acquire_write_buffer();
  if (index < 0) return true;
  i420_to_rgb565_swapped(output.outbuf, g_pixels[index]);
  if (g_stop_requested) {
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
      set_status("H.264 Decoderfehler", true);
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

static void camera_task(void*) {
  set_status("HTTP-Verbindung wird aufgebaut");
  const String url = g_url;
  NetworkClient plain_client;
  NetworkClientSecure secure_client;
  secure_client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(1000);
  http.setReuse(false);

  const bool secure = url.startsWith("https://");
  const bool began = secure ? http.begin(secure_client, url)
                            : http.begin(plain_client, url);
  if (!began) {
    set_status("Stream-URL konnte nicht geoeffnet werden", true);
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char message[64];
    snprintf(message, sizeof(message), "HTTP-Fehler %d", code);
    set_status(message, true);
    http.end();
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  esp_h264_dec_cfg_sw_t decoder_cfg{};
  decoder_cfg.pic_type = ESP_H264_RAW_FMT_I420;
  esp_h264_dec_handle_t decoder = nullptr;
  if (esp_h264_dec_sw_new(&decoder_cfg, &decoder) != ESP_H264_ERR_OK ||
      !decoder ||
      esp_h264_dec_open(decoder) != ESP_H264_ERR_OK) {
    set_status("H.264 Decoder konnte nicht starten", true);
    if (decoder) esp_h264_dec_del(decoder);
    http.end();
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  uint8_t* input = static_cast<uint8_t*>(heap_caps_malloc(
      kInputBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input) {
    set_status("Zu wenig PSRAM fuer Stream-Puffer", true);
    esp_h264_dec_del(decoder);
    http.end();
    g_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  NetworkClient* stream = http.getStreamPtr();
  size_t buffered = 0;
  uint32_t frame_count = 0;
  uint32_t last_fps_ms = millis();
  set_status("Puffern ...");

  while (!g_stop_requested && http.connected()) {
    const int available = stream ? stream->available() : 0;
    if (available <= 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    const size_t room = kInputBytes - buffered;
    if (room == 0) {
      set_status("H.264 Eingangspuffer voll", true);
      break;
    }
    const size_t wanted =
        std::min(room, static_cast<size_t>(available));
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
      snprintf(message, sizeof(message), "LIVE  %.1f FPS", fps);
      set_status(message);
      frame_count = 0;
      last_fps_ms = now;
    }
    taskYIELD();
  }

  heap_caps_free(input);
  esp_h264_dec_close(decoder);
  esp_h264_dec_del(decoder);
  http.end();
  if (g_stop_requested) set_status("Stream beendet");
  else if (!g_status_error) set_status("Streamverbindung beendet", true);
  g_task = nullptr;
  vTaskDelete(nullptr);
}

}  // namespace

bool camera_stream_start(const char* url) {
  if (!url || !*url) {
    set_status("Leere Stream-URL", true);
    return false;
  }
  if (g_task) {
    set_status("Kamerastream laeuft bereits", true);
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
  portEXIT_CRITICAL(&g_state_mux);

  g_url = url;
  g_stop_requested = false;
  g_worker_frame_count = 0;
  const BaseType_t task_core = (ARDUINO_RUNNING_CORE == 0) ? 1 : 0;
  if (xTaskCreatePinnedToCoreWithCaps(
          camera_task, "cameraH264", 16384, nullptr, tskIDLE_PRIORITY,
          &g_task, task_core, MALLOC_CAP_SPIRAM) != pdPASS) {
    g_task = nullptr;
    set_status("Kamera-Task konnte nicht starten", true);
    return false;
  }
  return true;
}

void camera_stream_stop() {
  g_stop_requested = true;
}

void camera_stream_process_ui(lv_obj_t* image,
                              lv_obj_t* placeholder,
                              lv_obj_t* status_label) {
  if (!image) return;
  int8_t ready = -1;
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

  if (ready >= 0) {
    lv_image_set_src(image, &g_images[ready]);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);
    if (placeholder) lv_obj_add_flag(placeholder, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(image);
  }

  char status[sizeof(g_status)];
  bool status_error = false;
  uint32_t status_sequence = 0;
  bool status_changed = false;
  portENTER_CRITICAL(&g_state_mux);
  status_sequence = g_status_sequence;
  if (status_sequence != g_ui_status_sequence) {
    snprintf(status, sizeof(status), "%s", g_status);
    status_error = g_status_error;
    g_ui_status_sequence = status_sequence;
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
  return g_task != nullptr;
}

#else

bool camera_stream_start(const char*) {
  camera_stream_set_external_status(
      "H.264-Test nur fuer Waveshare 8 Zoll", true);
  return false;
}

void camera_stream_stop() {}
void camera_stream_process_ui(lv_obj_t*, lv_obj_t*, lv_obj_t*) {}
void camera_stream_set_external_status(const char*, bool) {}
bool camera_stream_is_active() { return false; }

#endif
