#include "src/video/camera_stream.h"

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "src/core/config_manager.h"
#include "src/core/i18n.h"

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8)

#include <HTTPClient.h>
#include <NetworkClient.h>
#include <esp_h264_dec.h>
#include <esp_h264_dec_sw.h>
#include <lwip/sockets.h>

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
constexpr uint32_t kHttpHeaderTimeoutMs = 5000;
constexpr int kHttpReceiveBufferBytes = 8 * 1024;
constexpr size_t kMinCameraDmaFreeBytes = 24 * 1024;
constexpr char kAccessUnitFraming[] = "be32-access-unit";

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
  Serial.printf("[CameraStream] Status%s: %s\n",
                error ? " FEHLER" : "",
                text ? text : "");
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
  if (g_worker_frame_count == 1) {
    Serial.printf("[CameraStream] Erstes Bild dekodiert (%u Bytes I420)\n",
                  static_cast<unsigned>(output.out_size));
  }
  return true;
}

static bool decode_access_unit(esp_h264_dec_handle_t decoder,
                               uint8_t* input,
                               size_t access_unit_size) {
  size_t offset = 0;
  while (offset < access_unit_size && !g_stop_requested) {
    esp_h264_dec_in_frame_t in_frame{};
    in_frame.raw_data.buffer = input + offset;
    in_frame.raw_data.len =
        static_cast<uint32_t>(access_unit_size - offset);
    esp_h264_dec_out_frame_t out_frame{};
    const esp_h264_err_t result =
        esp_h264_dec_process(decoder, &in_frame, &out_frame);
    if (result != ESP_H264_ERR_OK) {
      Serial.printf(
          "[CameraStream] Decoderfehler=%d au=%u offset=%u consume=%u\n",
          static_cast<int>(result),
          static_cast<unsigned>(access_unit_size),
          static_cast<unsigned>(offset),
          static_cast<unsigned>(in_frame.consume));
      set_status(camera_text().camera_decoder_error, true);
      return false;
    }
    if (!output_frame(out_frame)) return false;
    if (in_frame.consume == 0) {
      Serial.printf(
          "[CameraStream] Decoder verbrauchte keine Daten: au=%u offset=%u\n",
          static_cast<unsigned>(access_unit_size),
          static_cast<unsigned>(offset));
      set_status(camera_text().camera_decoder_error, true);
      return false;
    }
    offset += in_frame.consume;
  }
  return true;
}

// Receives the HomeTiles record stream after HttpChunkedBodyDecoder has
// removed HTTP/1.1 chunk framing. Each record is a big-endian uint32 length
// followed by exactly one complete Annex-B H.264 access unit.
class H264AccessUnitStream final : public Stream {
 public:
  H264AccessUnitStream(esp_h264_dec_handle_t decoder, uint8_t* input)
      : decoder_(decoder),
        input_(input),
        last_fps_ms_(millis()) {}

  size_t write(uint8_t value) override {
    return write(&value, 1);
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!data || len == 0) return 0;
    if (g_stop_requested || failed_) return 0;
    const size_t accepted = len;

    while (len > 0 && !failed_ && !g_stop_requested) {
      if (length_bytes_ < sizeof(length_header_)) {
        const size_t take =
            std::min(len, sizeof(length_header_) - length_bytes_);
        memcpy(length_header_ + length_bytes_, data, take);
        length_bytes_ += take;
        data += take;
        len -= take;
        if (length_bytes_ < sizeof(length_header_)) continue;

        expected_bytes_ =
            (static_cast<size_t>(length_header_[0]) << 24) |
            (static_cast<size_t>(length_header_[1]) << 16) |
            (static_cast<size_t>(length_header_[2]) << 8) |
            static_cast<size_t>(length_header_[3]);
        buffered_ = 0;
        if (expected_bytes_ == 0) {
          length_bytes_ = 0;
          if (!received_flush_record_) {
            received_flush_record_ = true;
            Serial.println("[CameraStream] Bridge-Flush-Record empfangen");
          }
          continue;
        }
        if (expected_bytes_ > kInputBytes) {
          Serial.printf(
              "[CameraStream] H.264 Access Unit zu gross: %u > %u Bytes\n",
              static_cast<unsigned>(expected_bytes_),
              static_cast<unsigned>(kInputBytes));
          set_status(camera_text().camera_input_buffer_full, true);
          failed_ = true;
          break;
        }
      }

      const size_t take =
          std::min(len, expected_bytes_ - buffered_);
      memcpy(input_ + buffered_, data, take);
      buffered_ += take;
      data += take;
      len -= take;
      if (buffered_ < expected_bytes_) continue;

      if (!received_first_access_unit_) {
        received_first_access_unit_ = true;
        Serial.printf(
            "[CameraStream] Erste vollstaendige H.264 Access Unit: %u Bytes "
            "int=%uKB largest=%uKB psram=%uKB\n",
            static_cast<unsigned>(expected_bytes_),
            static_cast<unsigned>(
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U),
            static_cast<unsigned>(
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) /
                1024U),
            static_cast<unsigned>(
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U));
      }

      const uint32_t frames_before = g_worker_frame_count;
      if (!decode_access_unit(decoder_, input_, expected_bytes_)) {
        failed_ = true;
        break;
      }
      frame_count_ += g_worker_frame_count - frames_before;
      length_bytes_ = 0;
      expected_bytes_ = 0;
      buffered_ = 0;

      const uint32_t now = millis();
      if (now - last_fps_ms_ >= 2000) {
        const float fps =
            static_cast<float>(frame_count_) * 1000.0f /
            static_cast<float>(now - last_fps_ms_);
        char message[64];
        snprintf(message, sizeof(message), camera_text().camera_fps_fmt, fps);
        set_status(message);
        frame_count_ = 0;
        last_fps_ms_ = now;
      }
      taskYIELD();
    }
    return failed_ ? 0 : accepted;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

 private:
  esp_h264_dec_handle_t decoder_;
  uint8_t* input_;
  uint8_t length_header_[4] = {};
  size_t length_bytes_ = 0;
  size_t expected_bytes_ = 0;
  size_t buffered_ = 0;
  uint32_t frame_count_ = 0;
  uint32_t last_fps_ms_ = 0;
  bool received_flush_record_ = false;
  bool received_first_access_unit_ = false;
  bool failed_ = false;
};

class HttpChunkedBodyDecoder {
 public:
  bool feed(const uint8_t* raw, size_t len,
            H264AccessUnitStream& output) {
    size_t offset = 0;
    while (offset < len && state_ != State::Done) {
      switch (state_) {
        case State::SizeLine: {
          const char ch = static_cast<char>(raw[offset++]);
          if (ch == '\n') {
            if (!parse_size_line()) return fail("ungueltige Chunk-Groesse");
            size_line_len_ = 0;
            state_ = chunk_remaining_ == 0 ? State::Done : State::Data;
          } else if (ch != '\r') {
            if (size_line_len_ + 1 >= sizeof(size_line_)) {
              return fail("Chunk-Groessenzeile zu lang");
            }
            size_line_[size_line_len_++] = ch;
          }
          break;
        }
        case State::Data: {
          const size_t take =
              std::min(chunk_remaining_, len - offset);
          if (output.write(raw + offset, take) != take) return false;
          offset += take;
          chunk_remaining_ -= take;
          if (chunk_remaining_ == 0) state_ = State::DataCr;
          break;
        }
        case State::DataCr:
          if (raw[offset++] != '\r') {
            return fail("CR nach Chunk fehlt");
          }
          state_ = State::DataLf;
          break;
        case State::DataLf:
          if (raw[offset++] != '\n') {
            return fail("LF nach Chunk fehlt");
          }
          state_ = State::SizeLine;
          break;
        case State::Done:
          break;
      }
    }
    return state_ != State::Done;
  }

 private:
  enum class State : uint8_t {
    SizeLine,
    Data,
    DataCr,
    DataLf,
    Done,
  };

  bool parse_size_line() {
    size_t index = 0;
    while (index < size_line_len_ &&
           (size_line_[index] == ' ' || size_line_[index] == '\t')) {
      ++index;
    }
    uint32_t value = 0;
    bool have_digit = false;
    for (; index < size_line_len_; ++index) {
      const char ch = size_line_[index];
      if (ch == ';' || ch == ' ' || ch == '\t') break;
      uint8_t digit = 0;
      if (ch >= '0' && ch <= '9') {
        digit = static_cast<uint8_t>(ch - '0');
      } else if (ch >= 'a' && ch <= 'f') {
        digit = static_cast<uint8_t>(ch - 'a' + 10);
      } else if (ch >= 'A' && ch <= 'F') {
        digit = static_cast<uint8_t>(ch - 'A' + 10);
      } else {
        return false;
      }
      if (value > (UINT32_MAX - digit) / 16U) return false;
      value = value * 16U + digit;
      have_digit = true;
    }
    if (!have_digit) return false;
    chunk_remaining_ = value;
    return true;
  }

  bool fail(const char* reason) {
    Serial.printf("[CameraStream] HTTP-Chunk-Fehler: %s\n", reason);
    set_status(camera_text().camera_connection_ended, true);
    state_ = State::Done;
    return false;
  }

  State state_ = State::SizeLine;
  char size_line_[32] = {};
  size_t size_line_len_ = 0;
  size_t chunk_remaining_ = 0;
};

static void finish_camera_task(HTTPClient* http,
                               esp_h264_dec_handle_t decoder,
                               uint8_t* input) {
  Serial.printf(
      "[CameraStream] Task-Ende: stop=%s frames=%u int=%uKB "
      "largest=%uKB psram=%uKB\n",
      g_stop_requested ? "ja" : "nein",
      static_cast<unsigned>(g_worker_frame_count),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U),
      static_cast<unsigned>(
          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024U),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U));
  if (input) heap_caps_free(input);
  if (decoder) {
    esp_h264_dec_close(decoder);
    esp_h264_dec_del(decoder);
  }
  if (http) http->end();
}

static void run_camera_task() {
  set_status(camera_text().camera_http_connecting);
  const String url = g_url;
  if (!url.startsWith("http://")) {
    Serial.println(
        "[CameraStream] Abgelehnt: Kamera-Transport muss lokales HTTP sein");
    set_status(camera_text().camera_invalid_response, true);
    return;
  }
  NetworkClient plain_client;
  HTTPClient http;
  http.setConnectTimeout(4000);
  // Home Assistant may still be scheduling the route while the BambuLab
  // integration finishes a snapshot request. Keep this timeout separate from
  // the short streaming read waits configured after the response arrives.
  http.setTimeout(kHttpHeaderTimeoutMs);
  http.setReuse(false);
  const char* response_headers[] = {
      "Transfer-Encoding",
      "X-HomeTiles-Framing",
  };
  http.collectHeaders(response_headers, 2);

  Serial.printf(
      "[CameraStream] Start: transport=http url_len=%u int=%uKB largest=%uKB "
      "psram=%uKB\n",
      static_cast<unsigned>(url.length()),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U),
      static_cast<unsigned>(
          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024U),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U));
  const bool began = http.begin(plain_client, url);
  if (!began) {
    Serial.println("[CameraStream] HTTP begin() fehlgeschlagen");
    set_status(camera_text().camera_url_open_failed, true);
    finish_camera_task(&http, nullptr, nullptr);
    return;
  }

  const uint32_t get_started_ms = millis();
  const int code = http.GET();
  Serial.printf("[CameraStream] HTTP GET=%d nach %ums\n",
                code,
                static_cast<unsigned>(millis() - get_started_ms));
  if (code != HTTP_CODE_OK) {
    char message[64];
    snprintf(message, sizeof(message), camera_text().camera_http_error_fmt,
             code);
    set_status(message, true);
    finish_camera_task(&http, nullptr, nullptr);
    return;
  }
  const int receive_buffer_bytes = kHttpReceiveBufferBytes;
  const int receive_buffer_result = plain_client.setSocketOption(
      SOL_SOCKET, SO_RCVBUF, &receive_buffer_bytes,
      sizeof(receive_buffer_bytes));
  Serial.printf("[CameraStream] HTTP RX-Puffer: %d Bytes (set=%d)\n",
                receive_buffer_bytes, receive_buffer_result);
  const String transfer_encoding = http.header("Transfer-Encoding");
  const bool chunked =
      transfer_encoding.equalsIgnoreCase("chunked");
  const String framing = http.header("X-HomeTiles-Framing");
  Serial.printf("[CameraStream] HTTP-Transfer: %s\n",
                chunked ? "chunked" : "identity");
  Serial.printf("[CameraStream] H.264-Framing: %s\n", framing.c_str());
  if (!framing.equalsIgnoreCase(kAccessUnitFraming)) {
    Serial.println("[CameraStream] Bridge liefert kein Access-Unit-Framing");
    set_status(camera_text().camera_invalid_response, true);
    finish_camera_task(&http, nullptr, nullptr);
    return;
  }

  esp_h264_dec_cfg_sw_t decoder_cfg{};
  decoder_cfg.pic_type = ESP_H264_RAW_FMT_I420;
  esp_h264_dec_handle_t decoder = nullptr;
  if (esp_h264_dec_sw_new(&decoder_cfg, &decoder) != ESP_H264_ERR_OK ||
      !decoder ||
      esp_h264_dec_open(decoder) != ESP_H264_ERR_OK) {
    Serial.println("[CameraStream] H.264-Decoder konnte nicht starten");
    set_status(camera_text().camera_decoder_start_failed, true);
    if (decoder) esp_h264_dec_del(decoder);
    finish_camera_task(&http, nullptr, nullptr);
    return;
  }

  uint8_t* input = static_cast<uint8_t*>(heap_caps_malloc(
      kInputBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input) {
    Serial.printf("[CameraStream] Eingangspuffer (%u Bytes) fehlt\n",
                  static_cast<unsigned>(kInputBytes));
    set_status(camera_text().camera_input_memory_failed, true);
    finish_camera_task(&http, decoder, nullptr);
    return;
  }

  set_status(camera_text().camera_buffering);
  H264AccessUnitStream decode_stream(decoder, input);
  HttpChunkedBodyDecoder chunk_decoder;
  NetworkClient* stream = http.getStreamPtr();
  uint8_t raw[2048];
  bool stream_ok = true;
  while (!g_stop_requested && stream &&
         (http.connected() || stream->available() > 0)) {
    const size_t dma_free = heap_caps_get_free_size(
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (dma_free < kMinCameraDmaFreeBytes) {
      Serial.printf(
          "[CameraStream] Sicherheitsstopp: DMA frei nur %u KB; "
          "MQTT/WLAN bleiben aktiv\n",
          static_cast<unsigned>(dma_free / 1024U));
      set_status(camera_text().camera_connection_ended, true);
      stream_ok = false;
      break;
    }
    const int available = stream->available();
    if (available <= 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    const size_t wanted =
        std::min(sizeof(raw), static_cast<size_t>(available));
    const int received = stream->read(raw, wanted);
    if (received <= 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    if (chunked) {
      if (!chunk_decoder.feed(raw, static_cast<size_t>(received),
                              decode_stream)) {
        stream_ok = false;
        break;
      }
    } else if (decode_stream.write(
                   raw, static_cast<size_t>(received)) !=
               static_cast<size_t>(received)) {
      stream_ok = false;
      break;
    }
  }
  Serial.printf("[CameraStream] HTTP-Stream beendet: ok=%s\n",
                stream_ok ? "ja" : "nein");

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

static void camera_task(void*) {
  // Keep all C++ objects inside this scope. Their destructors must run before
  // FreeRTOS deletes the task stack, especially HTTPClient/NetworkClient.
  run_camera_task();

  bool release_buffers = false;
  portENTER_CRITICAL(&g_state_mux);
  g_task = nullptr;
  release_buffers = g_release_buffers;
  portEXIT_CRITICAL(&g_state_mux);
  if (release_buffers) release_frame_buffers();
  vTaskDelete(nullptr);
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
  Serial.printf(
      "[CameraStream] Bildpuffer bereit: %u x %u Bytes, int=%uKB "
      "largest=%uKB psram=%uKB\n",
      static_cast<unsigned>(kFrameBufferCount),
      static_cast<unsigned>(kPixelBytes),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U),
      static_cast<unsigned>(
          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024U),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U));

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
  const bool was_active = task_is_active();
  g_stop_requested = true;
  Serial.printf("[CameraStream] Stop angefordert (aktiv=%s)\n",
                was_active ? "ja" : "nein");
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
