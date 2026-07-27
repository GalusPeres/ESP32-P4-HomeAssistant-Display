#include "src/core/github_update.h"

#include <HTTPClient.h>
#include <Network.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <lwip/sockets.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>  // MBEDTLS_ERR_SSL_ALLOC_FAILED
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "src/core/firmware_metadata.h"
#include "src/core/firmware_version.h"
#include "src/devices/device.h"
#include "src/network/network_transport.h"

namespace {

constexpr size_t kInstallReadChunk = 2048;
constexpr size_t kInstallWriteSliceBytes = 16 * 1024;
constexpr size_t kSocketRxBufferBytes = 4 * 1024;
constexpr size_t kStageRangeBytes = 512 * 1024;
constexpr size_t kStageBlockBytes = kStageRangeBytes;
constexpr size_t kMaxStageBlocks = 32;
constexpr uint8_t kStageRangeAttempts = 3;
// Feldtest v0.5.4->v0.5.5: Nach ~4 MB Dauerempfang lief die ESP-Hosted
// RX-Queue voll ("Failed to push data to rx queue") und der C6 kippte um.
// Deshalb zwischen den Ranges ein echtes Erholungsfenster und beim Lesen
// staerker drosseln (2 ms je 2-KB-Chunk = max. ~1 MB/s statt ~2 MB/s).
constexpr uint32_t kStageRangePauseMs = 750;
constexpr size_t kMaxHttpLineLen = 4096;
constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kReadTimeoutMs = 20000;
constexpr uint32_t kReadPaceMs = 2;

class PsramStageBuffer {
 public:
  ~PsramStageBuffer() { release(); }

  bool allocate(size_t capacity) {
    release();
    if (capacity == 0) return true;

    const size_t required_blocks =
        capacity / kStageBlockBytes +
        ((capacity % kStageBlockBytes) != 0 ? 1 : 0);
    if (required_blocks == 0 || required_blocks > kMaxStageBlocks) {
      return false;
    }

    capacity_ = capacity;
    for (size_t i = 0; i < required_blocks; ++i) {
      const size_t offset = i * kStageBlockBytes;
      const size_t block_bytes =
          std::min(kStageBlockBytes, capacity - offset);
      blocks_[i] = static_cast<uint8_t*>(heap_caps_malloc(
          block_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
      if (!blocks_[i]) {
        release();
        return false;
      }
      ++block_count_;
    }
    return true;
  }

  void release() {
    for (size_t i = 0; i < block_count_; ++i) {
      heap_caps_free(blocks_[i]);
      blocks_[i] = nullptr;
    }
    block_count_ = 0;
    capacity_ = 0;
  }

  uint8_t* block(size_t index) const {
    return index < block_count_ ? blocks_[index] : nullptr;
  }

  size_t blockSize(size_t index) const {
    if (index >= block_count_) return 0;
    const size_t offset = index * kStageBlockBytes;
    return std::min(kStageBlockBytes, capacity_ - offset);
  }

  size_t blockCount() const { return block_count_; }

 private:
  uint8_t* blocks_[kMaxStageBlocks] = {};
  size_t block_count_ = 0;
  size_t capacity_ = 0;
};

// Der vorgebaute ESP32-P4-Arduino-Core ist mit
// CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC gebaut. Dadurch landen selbst die grossen,
// nur fuer einen HTTPS-Handshake benoetigten mbedTLS-Bloecke im knappen
// internen RAM, obwohl reichlich PSRAM frei ist. Fuer den kurzen
// Versions-Check darf mbedTLS deshalb PSRAM bevorzugen. Der Fallback auf den
// normalen ESP-Allocator bleibt erhalten, falls eine Allokation aus PSRAM
// wider Erwarten nicht moeglich ist.
void* checkTlsInternalCalloc(size_t count, size_t size) {
  return heap_caps_calloc(count, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void* checkTlsPsramCalloc(size_t count, size_t size) {
  void* ptr = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return ptr ? ptr : checkTlsInternalCalloc(count, size);
}

void checkTlsHeapFree(void* ptr) {
  heap_caps_free(ptr);
}

class ScopedCheckTlsPsramAllocator {
 public:
  ScopedCheckTlsPsramAllocator()
      : active_(mbedtls_platform_set_calloc_free(checkTlsPsramCalloc,
                                                 checkTlsHeapFree) == 0) {}

  ~ScopedCheckTlsPsramAllocator() {
    if (active_) {
      // Entspricht CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC des verwendeten
      // Arduino-Cores.
      mbedtls_platform_set_calloc_free(checkTlsInternalCalloc,
                                       checkTlsHeapFree);
    }
  }

  bool active() const { return active_; }

 private:
  bool active_;
};

struct ParsedHttpsUrl {
  String host;
  String path;
  uint16_t port = 443;
};

// "v0.3.1" / "0.3.1" -> {0,3,1}; fehlende Teile bleiben 0.
void parseVersion(const char* s, int out[3]) {
  out[0] = out[1] = out[2] = 0;
  if (!s) return;
  while (*s && !isdigit(static_cast<unsigned char>(*s))) ++s;
  for (int i = 0; i < 3 && *s; ++i) {
    out[i] = atoi(s);
    while (*s && isdigit(static_cast<unsigned char>(*s))) ++s;
    if (*s != '.') break;
    ++s;
  }
}

bool isNewerThanCurrent(const char* tag) {
  int latest[3];
  int current[3];
  parseVersion(tag, latest);
  parseVersion(FW_VERSION, current);
  for (int i = 0; i < 3; ++i) {
    if (latest[i] != current[i]) return latest[i] > current[i];
  }
  return false;
}

bool parseHttpsUrl(const String& url, ParsedHttpsUrl& out) {
  constexpr const char* kPrefix = "https://";
  if (!url.startsWith(kPrefix)) return false;

  const int host_start = strlen(kPrefix);
  int path_start = url.indexOf('/', host_start);
  if (path_start < 0) path_start = url.length();

  String host_port = url.substring(host_start, path_start);
  if (!host_port.length()) return false;

  out.port = 443;
  const int colon = host_port.lastIndexOf(':');
  if (colon > 0) {
    out.host = host_port.substring(0, colon);
    const int parsed_port = atoi(host_port.substring(colon + 1).c_str());
    if (parsed_port <= 0 || parsed_port > 65535) return false;
    out.port = static_cast<uint16_t>(parsed_port);
  } else {
    out.host = host_port;
  }

  out.path = (path_start < url.length()) ? url.substring(path_start) : "/";
  return out.host.length() && out.path.length();
}

bool readHttpLine(NetworkClientSecure& client, String& line, uint32_t timeout_ms) {
  line = "";
  const uint32_t start_ms = millis();
  while (millis() - start_ms < timeout_ms) {
    while (client.available() > 0) {
      const char c = static_cast<char>(client.read());
      if (c == '\r') continue;
      if (c == '\n') return true;
      if (line.length() < kMaxHttpLineLen) line += c;
    }
    if (!client.connected() && !client.available()) return line.length() > 0;
    delay(1);
  }
  return false;
}

bool parseStatusCode(const String& status_line, int& code) {
  const int first_space = status_line.indexOf(' ');
  if (first_space < 0 || first_space + 4 > status_line.length()) return false;
  code = atoi(status_line.substring(first_space + 1, first_space + 4).c_str());
  return code > 0;
}

size_t parseSizeT(const String& value) {
  return static_cast<size_t>(strtoul(value.c_str(), nullptr, 10));
}

bool parseContentRangeTotal(const String& value, size_t& total) {
  const int slash = value.lastIndexOf('/');
  if (slash < 0 || slash + 1 >= value.length()) return false;
  const String total_part = value.substring(slash + 1);
  if (total_part == "*") return false;
  total = parseSizeT(total_part);
  return total > 0;
}

String resolveRedirectUrl(const String& current_url,
                          const ParsedHttpsUrl& current,
                          const String& location) {
  if (location.startsWith("https://")) return location;
  if (location.startsWith("//")) return String("https:") + location;
  if (location.startsWith("/")) {
    return String("https://") + current.host + location;
  }

  const int slash = current_url.lastIndexOf('/');
  if (slash > strlen("https://")) {
    return current_url.substring(0, slash + 1) + location;
  }
  return String("https://") + current.host + "/" + location;
}

String releaseDownloadUrl(const char* tag, const String& asset_name) {
  return String(GithubUpdate::kRepoUrl) + "/releases/download/" + tag + "/" + asset_name;
}

String legacyDeviceSlug() {
  String slug = Device::profile().key;
  slug.replace('_', '-');
  return slug;
}

void logCheckNetworkState(const char* label, const String& url) {
  const uint32_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const uint32_t dma_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t dma_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

  Serial.printf("[Update/Diag] %s url=%s\n",
                label ? label : "?",
                url.c_str());
  const long rssi = networkTransport.activeKind() == NetworkTransportKind::Wifi
                        ? static_cast<long>(WiFi.RSSI())
                        : 0;
  Serial.printf(
      "[Update/Diag] transport=%s connected=%d ip=%s gw=%s dns=%s rssi=%ld\n",
      networkTransport.activeName(),
      networkTransport.isConnected() ? 1 : 0,
      networkTransport.localIP().toString().c_str(),
      networkTransport.gatewayIP().toString().c_str(),
      networkTransport.dnsIP(0).toString().c_str(),
      rssi);
  Serial.printf("[Update/Diag] mem int=%u KB largest=%u KB dma=%u KB dma_largest=%u KB psram=%u KB\n",
                static_cast<unsigned>(int_free / 1024),
                static_cast<unsigned>(int_largest / 1024),
                static_cast<unsigned>(dma_free / 1024),
                static_cast<unsigned>(dma_largest / 1024),
                static_cast<unsigned>(ESP.getFreePsram() / 1024));
}

// Forensik fuer den sicheren Neustart nach fehlgeschlagenem Install: Der
// Neustart hinterlaesst keinen Core-Dump. install() sammelt hier deshalb die
// Details (Range, Versuch, Fehler, Speicher-/WLAN-Lage), und der Aufrufer
// haengt sie vor dem Restart an den Absturzbericht (/crashlog.txt) an.
String g_install_diag;
constexpr size_t kInstallDiagMaxBytes = 1536;
// True, sobald Asset und Firmware-Metadaten validiert sind: Jeder spaetere
// Fehler ist ein Transportproblem (ESP-Hosted-Abriss, CDN), kein inhaltliches
// - nur dann lohnt ein automatischer neuer Versuch nach dem Neustart.
bool g_install_retryable = false;

void installDiagLine(const String& line) {
  if (g_install_diag.length() + line.length() + 1 > kInstallDiagMaxBytes) {
    return;  // Bericht bleibt begrenzt; die ersten Fehler sind die wichtigsten
  }
  g_install_diag += line;
  g_install_diag += '\n';
}

String memSnapshotLine() {
  char buf[112];
  snprintf(buf, sizeof(buf),
           "mem int=%uKB largest=%uKB dma_largest=%uKB psram_largest=%uKB",
           static_cast<unsigned>(
               heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           static_cast<unsigned>(
               heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
           static_cast<unsigned>(heap_caps_get_largest_free_block(
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) /
                                 1024),
           static_cast<unsigned>(
               heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
  return String(buf);
}

// Gibt den mbedTLS-Fehlercode zurueck (0 = keiner), damit der Aufrufer
// Speicher-Fehlschlaege (MBEDTLS_ERR_SSL_ALLOC_FAILED) erkennen kann.
int logCheckFailureDetails(int code, NetworkClientSecure& client, const String& url) {
  char tls_error[96] = {};
  const int tls_code = client.lastError(tls_error, sizeof(tls_error));

  IPAddress github_ip;
  const int dns_ok = Network.hostByName("github.com", github_ip) ? 1 : 0;

  if (code < 0) {
    Serial.printf("[Update/Diag] http=%d (%s) tls=%d (%s) fd=%d\n",
                  code,
                  HTTPClient::errorToString(code).c_str(),
                  tls_code,
                  tls_error,
                  client.fd());
  } else {
    Serial.printf("[Update/Diag] http=%d tls=%d (%s) fd=%d\n",
                  code,
                  tls_code,
                  tls_error,
                  client.fd());
  }
  Serial.printf("[Update/Diag] dns github.com: ok=%d ip=%s | url=%s\n",
                dns_ok,
                github_ip.toString().c_str(),
                url.c_str());
  return tls_code;
}

typedef bool (*RangeDataFn)(const uint8_t* data, size_t len, void* ctx);

bool fetchHttpRange(const String& start_url, size_t from, size_t to,
                    uint8_t* buf, size_t buf_len, RangeDataFn on_data,
                    void* ctx, size_t& content_total, String& error_out,
                    String* final_url_out = nullptr) {
  if (!buf || !buf_len || !on_data || to < from) {
    error_out = "invalid range request";
    return false;
  }

  String url = start_url;
  for (int redirect = 0; redirect < 5; ++redirect) {
    ParsedHttpsUrl parsed;
    if (!parseHttpsUrl(url, parsed)) {
      error_out = "bad https url";
      return false;
    }

    NetworkClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);
    if (!client.connect(parsed.host.c_str(), parsed.port, kConnectTimeoutMs)) {
      error_out = String("connect failed: ") + parsed.host;
      return false;
    }

    int rcvbuf = static_cast<int>(kSocketRxBufferBytes);
    if (client.fd() >= 0) {
      client.setSocketOption(SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    }

    client.printf("GET %s HTTP/1.1\r\n", parsed.path.c_str());
    client.printf("Host: %s\r\n", parsed.host.c_str());
    client.print("User-Agent: hometiles\r\n");
    client.print("Accept: application/octet-stream\r\n");
    client.print("Accept-Encoding: identity\r\n");
    client.printf("Range: bytes=%u-%u\r\n",
                  static_cast<unsigned>(from), static_cast<unsigned>(to));
    client.print("Connection: close\r\n\r\n");

    String line;
    if (!readHttpLine(client, line, kReadTimeoutMs)) {
      client.stop();
      error_out = "no http status";
      return false;
    }

    int status_code = 0;
    if (!parseStatusCode(line, status_code)) {
      client.stop();
      error_out = "bad http status";
      return false;
    }

    String location;
    size_t content_length = 0;
    bool has_content_length = false;
    size_t range_total = 0;

    bool header_complete = false;
    while (readHttpLine(client, line, kReadTimeoutMs)) {
      if (!line.length()) {
        header_complete = true;
        break;
      }
      const int colon = line.indexOf(':');
      if (colon <= 0) continue;
      String name = line.substring(0, colon);
      name.toLowerCase();
      String value = line.substring(colon + 1);
      value.trim();
      if (name == "location") {
        location = value;
      } else if (name == "content-length") {
        content_length = parseSizeT(value);
        has_content_length = content_length > 0;
      } else if (name == "content-range") {
        parseContentRangeTotal(value, range_total);
      }
    }
    if (!header_complete) {
      client.stop();
      error_out = "header timeout";
      return false;
    }

    if (status_code >= 300 && status_code < 400 && location.length()) {
      client.stop();
      url = resolveRedirectUrl(url, parsed, location);
      continue;
    }

    if (status_code != 206) {
      client.stop();
      error_out = String("HTTP ") + status_code;
      return false;
    }
    if (!range_total) {
      client.stop();
      error_out = "missing content-range";
      return false;
    }
    content_total = range_total;

    const size_t expected = to - from + 1;
    if (has_content_length && content_length != expected) {
      client.stop();
      error_out = "range length mismatch";
      return false;
    }

    size_t received = 0;
    uint32_t last_data_ms = millis();
    while (received < expected) {
      const int avail = client.available();
      if (avail > 0) {
        size_t want = expected - received;
        if (want > buf_len) want = buf_len;
        if (want > static_cast<size_t>(avail)) want = static_cast<size_t>(avail);

        const int r = client.read(buf, want);
        if (r <= 0) {
          delay(1);
          continue;
        }
        if (!on_data(buf, static_cast<size_t>(r), ctx)) {
          client.stop();
          if (!error_out.length()) error_out = "write failed";
          return false;
        }
        received += static_cast<size_t>(r);
        last_data_ms = millis();
        if (kReadPaceMs > 0) {
          delay(kReadPaceMs);
        } else {
          yield();
        }
      } else {
        if (!client.connected() || millis() - last_data_ms > kReadTimeoutMs) {
          client.stop();
          error_out = client.connected() ? "timeout" : "connection lost";
          return false;
        }
        delay(10);
      }
    }

    client.stop();
    if (final_url_out) *final_url_out = url;
    return true;
  }

  error_out = "too many redirects";
  return false;
}

struct HeadBufferCtx {
  uint8_t* data = nullptr;
  size_t capacity = 0;
  size_t len = 0;
  // Nur fuer den Stage-Download gesetzt: meldet waehrend des Ladens
  // Fortschritt, damit der Aufrufer (Activity-Timer/Status) weiterlaeuft.
  size_t progress_base = 0;
  size_t progress_total = 0;
  uint32_t last_progress_ms = 0;
  GithubUpdate::ProgressFn progress = nullptr;
};

bool storeHeadBytes(const uint8_t* data, size_t len, void* raw_ctx) {
  auto* ctx = static_cast<HeadBufferCtx*>(raw_ctx);
  if (!ctx || !ctx->data || ctx->len + len > ctx->capacity) return false;
  memcpy(ctx->data + ctx->len, data, len);
  ctx->len += len;
  return true;
}

bool storeBufferBytes(const uint8_t* data, size_t len, void* raw_ctx) {
  auto* ctx = static_cast<HeadBufferCtx*>(raw_ctx);
  if (!ctx || !ctx->data || ctx->len + len > ctx->capacity) return false;
  memcpy(ctx->data + ctx->len, data, len);
  ctx->len += len;
  if (ctx->progress) {
    const uint32_t now_ms = millis();
    if (now_ms - ctx->last_progress_ms >= 500) {
      ctx->last_progress_ms = now_ms;
      ctx->progress(ctx->progress_base + ctx->len, ctx->progress_total);
    }
  }
  return true;
}

struct UpdateWriteCtx {
  size_t written = 0;
  size_t total = 0;
  size_t next_progress_log = 512 * 1024;
  uint32_t last_progress_ms = 0;
  GithubUpdate::ProgressFn progress = nullptr;
  String* error = nullptr;
};

void reportInstallProgress(UpdateWriteCtx& ctx, bool force) {
  if (!ctx.progress) return;
  const uint32_t now_ms = millis();
  if (force || now_ms - ctx.last_progress_ms >= 500 ||
      (ctx.total && ctx.written >= ctx.total)) {
    ctx.last_progress_ms = now_ms;
    ctx.progress(ctx.written, ctx.total);
  }
}

bool writeUpdateBytes(const uint8_t* data, size_t len, void* raw_ctx) {
  auto* ctx = static_cast<UpdateWriteCtx*>(raw_ctx);
  if (!ctx || !data) return false;
  // In Scheiben schreiben statt der ganzen Stage am Stueck: waehrend
  // Update.write blockiert der Loop-Task sekundenlang im Flash-Erase/Write
  // und der esp-hosted-SDIO-Treiber kann eingehende WLAN-Frames nicht mehr
  // ablegen -> assert pkt_rxbuff (sdio_drv.c:928) mitten im Install. Die
  // Pausen zwischen den Scheiben lassen ihn die RX-Queue leeren; mit den
  // alten 32KB-Stages war das Fenster kurz genug, mit 512KB nicht mehr.
  size_t offset = 0;
  while (offset < len) {
    size_t slice = len - offset;
    if (slice > kInstallWriteSliceBytes) slice = kInstallWriteSliceBytes;
    const size_t bytes_written =
        Update.write(const_cast<uint8_t*>(data + offset), slice);
    if (bytes_written != slice) {
      if (ctx->error) *ctx->error = Update.errorString();
      return false;
    }
    ctx->written += bytes_written;
    offset += slice;
    if (offset < len) delay(2);
  }
  if (ctx->written >= ctx->next_progress_log ||
      (ctx->total && ctx->written >= ctx->total)) {
    Serial.printf("[Update] Install progress: %u / %u bytes\n",
                  static_cast<unsigned>(ctx->written),
                  static_cast<unsigned>(ctx->total));
    ctx->next_progress_log += 512 * 1024;
  }
  reportInstallProgress(*ctx, false);
  return true;
}

}  // namespace

namespace GithubUpdate {

CheckResult checkLatest() {
  CheckResult result;
  if (!networkTransport.isConnected()) {
    Serial.println("[Update] Check uebersprungen: kein Netzwerk");
    return result;
  }

  // Der Guard lebt laenger als NetworkClientSecure und HTTPClient. Deren
  // Destruktoren geben daher alle PSRAM-basierten TLS-Bloecke frei, bevor der
  // globale mbedTLS-Allocator wieder auf den Core-Standard zurueckgestellt
  // wird. Beide Free-Funktionen verwenden den ESP-Heap und koennen sowohl
  // interne als auch externe Bloecke freigeben.
  ScopedCheckTlsPsramAllocator tls_allocator;
  Serial.printf("[Update] Check: TLS-Allokationen %s\n",
                tls_allocator.active() ? "PSRAM bevorzugt" : "Core-Standard");

  // GitHub- und CDN-Zertifikate rotieren regelmaessig; eine eingebrannte
  // CA-Liste waere beim ersten Wechsel tot. Fuer Firmware von der eigenen
  // Release-Seite ist der Verzicht auf die Pruefung der uebliche Kompromiss
  // (Update.end validiert das Image selbst via Magic-Byte + Groesse).
  NetworkClientSecure client;
  client.setInsecure();

  // Redirects NICHT pauschal folgen: der Location-Header traegt den Tag.
  // Nach einem Repo-Rename liefert GitHub aber ERST eine Umleitung auf den
  // neuen Repo-Pfad (.../releases/latest) - dieser Kette manuell folgen,
  // bis eine Tag-URL auftaucht, sonst verlieren ausgelieferte Geraete beim
  // Umbenennen des Repos dauerhaft ihre Update-Suche.
  String url = String(kRepoUrl) + "/releases/latest";
  String tag;
  logCheckNetworkState("check-start", url);
  for (int hop = 0; hop < 4; ++hop) {
    client.stop();
    client.setTimeout(8000);
    client.setHandshakeTimeout(12);

    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    http.setReuse(false);
    if (!http.begin(client, url)) {
      Serial.printf("[Update] Check fehlgeschlagen: HTTP begin (%s)\n", url.c_str());
      logCheckFailureDetails(0, client, url);
      client.stop();
      return result;
    }
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Connection", "close");
    const char* kCollect[] = {"Location"};
    http.collectHeaders(kCollect, 1);

    const int code = http.GET();
    const String location = http.header("Location");
    http.end();
    client.stop();
    delay(10);

    if (code < 300 || code >= 400 || !location.length()) {
      if (code < 0) {
        Serial.printf("[Update] Check fehlgeschlagen: HTTP %d (%s)\n",
                      code,
                      HTTPClient::errorToString(code).c_str());
      } else {
        Serial.printf("[Update] Check fehlgeschlagen: HTTP %d\n", code);
      }
      result.tls_alloc_failed =
          logCheckFailureDetails(code, client, url) == MBEDTLS_ERR_SSL_ALLOC_FAILED;
      return result;
    }

    const int tag_pos = location.indexOf("/releases/tag/");
    if (tag_pos >= 0) {
      tag = location.substring(tag_pos + strlen("/releases/tag/"));
      break;
    }
    if (location.indexOf("/releases/latest") < 0) {
      // Weder Tag- noch latest-Pfad: Repo ohne Releases (leitet auf
      // /releases um) oder etwas Unerwartetes.
      Serial.printf("[Update] Kein Release gefunden (%s)\n", location.c_str());
      return result;
    }
    url = location;  // Rename-Redirect: neue Repo-URL erneut anfragen
  }

  if (!tag.length() || tag.length() >= sizeof(result.latest_tag)) {
    Serial.printf("[Update] Unerwarteter Tag: '%s'\n", tag.c_str());
    return result;
  }

  snprintf(result.latest_tag, sizeof(result.latest_tag), "%s", tag.c_str());
  result.ok = true;
  result.update_available = isNewerThanCurrent(result.latest_tag);
  Serial.printf("[Update] Installiert %s, neuestes Release %s -> %s\n",
                FW_VERSION, result.latest_tag,
                result.update_available ? "Update verfügbar" : "aktuell");
  return result;
}

bool install(const char* tag, ProgressFn progress, String& error_out) {
  error_out = "";
  if (!tag || !*tag) {
    error_out = "no tag";
    return false;
  }
  if (!networkTransport.isConnected()) {
    error_out = "no network";
    return false;
  }
  // Evtl. haengengebliebenen Web-OTA-Rest aufraeumen
  if (Update.isRunning()) Update.abort();

  String url = releaseDownloadUrl(
      tag, String("hometiles_") + tag + "_" + Device::profile().key + ".bin");
  Serial.printf("[Update] Lade %s\n", url.c_str());

  g_install_diag = "";
  g_install_retryable = false;
  String transport_diag =
      String("Start: Uptime ") + (millis() / 1000) + " s | " +
      networkTransport.activeName();
  if (networkTransport.activeKind() == NetworkTransportKind::Wifi) {
    transport_diag += " RSSI ";
    transport_diag += WiFi.RSSI();
    transport_diag += " dBm";
  }
  installDiagLine(transport_diag + " | " + memSnapshotLine());

  // Netzwerk-Lesepuffer und OTA-Staging liegen im PSRAM. Das VOLLSTAENDIGE
  // Image wird vor Update.begin() heruntergeladen. Erst nachdem TLS beendet
  // und sein interner AES/DMA-Speicher wieder frei ist, beginnt der
  // Flash-Schreibvorgang. So konkurrieren ESP-Hosted-RX, mbedTLS und der
  // verschluesselte OTA-Writer nie gleichzeitig um das knappe interne RAM.
  uint8_t* net_buf = static_cast<uint8_t*>(
      heap_caps_malloc(kInstallReadChunk, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!net_buf) net_buf = static_cast<uint8_t*>(malloc(kInstallReadChunk));
  PsramStageBuffer stage;
  if (!net_buf) {
    error_out = "alloc failed";
    return false;
  }

  bool failed = false;
  bool update_started = false;
  size_t total_sz = 0;

  uint8_t image_head[firmware_meta::kDeviceDescriptorImageBytes] = {0};
  HeadBufferCtx head_ctx{image_head, sizeof(image_head), 0};
  String resolved_asset_url;
  if (!fetchHttpRange(url, 0, sizeof(image_head) - 1, net_buf, kInstallReadChunk,
                      storeHeadBytes, &head_ctx, total_sz, error_out,
                      &resolved_asset_url)) {
    const String first_error = error_out;
    const String legacy_url = releaseDownloadUrl(
        tag,
        String("esp32-p4-homeassistant-display-") + tag + "-" +
            legacyDeviceSlug() + "-update.bin");
    memset(image_head, 0, sizeof(image_head));
    head_ctx.len = 0;
    total_sz = 0;
    error_out = "";
    resolved_asset_url = "";
    Serial.printf("[Update] Asset nicht gefunden/lesbar (%s), versuche %s\n",
                  first_error.c_str(), legacy_url.c_str());
    if (!fetchHttpRange(legacy_url, 0, sizeof(image_head) - 1, net_buf,
                        kInstallReadChunk, storeHeadBytes, &head_ctx, total_sz,
                        error_out, &resolved_asset_url)) {
      error_out = first_error + "; fallback: " + error_out;
      failed = true;
    } else {
      url = legacy_url;
    }
  }
  if (!failed && resolved_asset_url.length()) {
    // Fuer alle weiteren Ranges direkt die bereits aufgeloeste CDN-URL
    // verwenden. Das spart pro Block den GitHub-Redirect samt TLS-Verbindung.
    url = resolved_asset_url;
  }
  if (!failed) {
    delay(20);
  }

  if (!failed) {
    if (total_sz < firmware_meta::kDeviceDescriptorImageBytes) {
      error_out = "image too small";
      failed = true;
    }
  }

  if (!failed) {
    firmware_meta::DeviceDescriptor incoming_desc{};
    if (!firmware_meta::parseDeviceDescriptorFromImage(
            image_head, head_ctx.len, incoming_desc)) {
      error_out = "firmware metadata missing or invalid";
      failed = true;
    } else if (!firmware_meta::matchesCurrentDeviceKey(incoming_desc.device_key)) {
      error_out = String("device mismatch: got ") +
                  incoming_desc.display_name + ", expected " +
                  firmware_meta::expectedDeviceDisplayName();
      failed = true;
    } else if (strcmp(incoming_desc.project_key,
                      firmware_meta::currentProjectKey()) != 0) {
      error_out = String("project mismatch: got ") +
                  incoming_desc.project_key + ", expected " +
                  firmware_meta::currentProjectKey();
      failed = true;
    }
  }

  if (!failed) {
    g_install_retryable = true;
  }

  size_t stage_capacity = 0;
  size_t staged_len = 0;
  if (!failed) {
    // Download und Flash-Schreiben bleiben strikt getrennt. Mehrere feste
    // PSRAM-Bloecke entfernen jedoch die unnoetige Forderung nach einem
    // einzelnen zusammenhaengenden ~6-MB-Block, die nach langer Laufzeit trotz
    // genuegend freiem PSRAM scheitern konnte.
    stage_capacity = total_sz - head_ctx.len;
    if (!stage.allocate(stage_capacity)) {
      error_out = "not enough PSRAM for safe OTA staging";
      Serial.printf(
          "[Update] PSRAM staging failed: need=%uKB free=%uKB "
          "largest=%uKB block=%uKB\n",
          static_cast<unsigned>(stage_capacity / 1024),
          static_cast<unsigned>(
              heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
          static_cast<unsigned>(
              heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024),
          static_cast<unsigned>(kStageBlockBytes / 1024));
      failed = true;
    } else {
      Serial.printf("[Update] Safe staged download: %uB TCP RX, %uB Lesechunk, %uKB Ranges, %uKB PSRAM in %u Bloecken\n",
                    static_cast<unsigned>(kSocketRxBufferBytes),
                    static_cast<unsigned>(kInstallReadChunk),
                    static_cast<unsigned>(kStageRangeBytes / 1024),
                    static_cast<unsigned>(stage_capacity / 1024),
                    static_cast<unsigned>(stage.blockCount()));
    }
  }

  // Den kompletten Rest laden, solange Update/Flash noch unangetastet sind.
  // Mehrere begrenzte Range-Requests verhindern, dass ESP-Hosted minutenlang
  // einen einzigen RX-Strom abarbeiten muss. Jeder Block darf neu gestartet
  // werden, ohne bereits geladene Daten oder das Flash zu veraendern.
  if (!failed) {
    while (staged_len < stage_capacity && !failed) {
      const size_t range_start = head_ctx.len + staged_len;
      size_t range_len = stage_capacity - staged_len;
      if (range_len > kStageRangeBytes) range_len = kStageRangeBytes;
      const size_t range_end = range_start + range_len - 1;
      bool range_ok = false;
      const size_t block_index = staged_len / kStageBlockBytes;
      uint8_t* const stage_block = stage.block(block_index);
      if (!stage_block || range_len > stage.blockSize(block_index)) {
        error_out = "invalid OTA staging block";
        failed = true;
        break;
      }

      for (uint8_t attempt = 1; attempt <= kStageRangeAttempts; ++attempt) {
        size_t range_total = 0;
        HeadBufferCtx stage_ctx{stage_block, range_len, 0};
        stage_ctx.progress_base = range_start;
        stage_ctx.progress_total = total_sz;
        stage_ctx.progress = progress;
        error_out = "";

        Serial.printf("[Update] Lade Range %u-%u (Versuch %u/%u)\n",
                      static_cast<unsigned>(range_start),
                      static_cast<unsigned>(range_end),
                      static_cast<unsigned>(attempt),
                      static_cast<unsigned>(kStageRangeAttempts));
        if (fetchHttpRange(url, range_start, range_end, net_buf,
                           kInstallReadChunk, storeBufferBytes, &stage_ctx,
                           range_total, error_out) &&
            range_total == total_sz && stage_ctx.len == range_len) {
          range_ok = true;
          break;
        }

        if (!error_out.length()) {
          error_out = (range_total && range_total != total_sz)
                          ? "image size changed"
                          : "range incomplete";
        }
        Serial.printf("[Update] Range fehlgeschlagen: %s\n", error_out.c_str());
        installDiagLine(String("Range ") + range_start + "-" + range_end +
                        " Versuch " + attempt + ": " + error_out + " | " +
                        memSnapshotLine());
        // Nach einem Streamabriss braucht der ESP-Hosted-Link deutlich
        // laenger als ein paar hundert Millisekunden, um Queues und Mempool
        // zu leeren. Sofortige Reconnects scheitern sonst direkt wieder
        // ("connect failed" im Feldtest).
        if (attempt < kStageRangeAttempts) delay(3000);
      }

      if (!range_ok) {
        failed = true;
        break;
      }

      staged_len += range_len;
      if (progress) progress(head_ctx.len + staged_len, total_sz);
      Serial.printf("[Update] Download progress: %u / %u Bytes\n",
                    static_cast<unsigned>(head_ctx.len + staged_len),
                    static_cast<unsigned>(total_sz));
      if (staged_len < stage_capacity) delay(kStageRangePauseMs);
    }

    if (!failed) {
      Serial.printf("[Update] Download vollstaendig im PSRAM: %u / %u Bytes\n",
                    static_cast<unsigned>(head_ctx.len + staged_len),
                    static_cast<unsigned>(total_sz));
    }
  }

  // Der HTTPS-Client ist nach fetchHttpRange bereits zerstoert. Auch den
  // letzten Lesepuffer vor Update.begin freigeben und dem SDIO-Task kurz Zeit
  // geben, bereits empfangene Pakete abzuarbeiten.
  free(net_buf);
  net_buf = nullptr;
  if (!failed) delay(50);

  if (!failed) {
    if (!Update.begin(total_sz, U_FLASH)) {
      error_out = Update.errorString();
      failed = true;
      Serial.printf("[Update] Update.begin fehlgeschlagen: %s\n",
                    error_out.c_str());
    } else {
      update_started = true;
      Serial.printf("[Update] Update.begin OK, Groesse: %u\n",
                    static_cast<unsigned>(total_sz));
      UpdateWriteCtx write_ctx;
      write_ctx.written = 0;
      write_ctx.total = total_sz;
      write_ctx.progress = progress;
      write_ctx.error = &error_out;
      if (!writeUpdateBytes(image_head, head_ctx.len, &write_ctx)) {
        if (!error_out.length()) error_out = Update.errorString();
        failed = true;
      } else {
        reportInstallProgress(write_ctx, true);
      }

      for (size_t i = 0; !failed && i < stage.blockCount(); ++i) {
        if (!writeUpdateBytes(stage.block(i), stage.blockSize(i),
                              &write_ctx)) {
          if (!error_out.length()) error_out = Update.errorString();
          failed = true;
        }
      }
    }
  }

  stage.release();
  if (net_buf) free(net_buf);

  if (failed) {
    if (update_started) Update.abort();
    Serial.printf("[Update] Install fehlgeschlagen: %s\n", error_out.c_str());
    installDiagLine(String("Abbruch bei ") + (head_ctx.len + staged_len) +
                    " / " + total_sz + " Bytes: " + error_out);
    return false;
  }

  if (!Update.end(true)) {
    error_out = Update.errorString();
    Serial.printf("[Update] Update.end fehlgeschlagen: %s\n", error_out.c_str());
    return false;
  }
  Serial.printf("[Update] %u Bytes installiert - bereit zum Neustart\n",
                static_cast<unsigned>(total_sz));
  return true;
}

String lastInstallDiag() { return g_install_diag; }

bool lastInstallRetryable() { return g_install_retryable; }

}  // namespace GithubUpdate
