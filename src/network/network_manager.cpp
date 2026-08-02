#include "src/network/network_manager.h"
#include "src/network/network_transport.h"
#include "src/core/config_manager.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/network/ha_bridge_config.h"
#include "src/web/web_admin.h"
#include "src/ui/ui_manager.h"
#include "src/ui/tab_settings.h"
#include "src/devices/device.h"
#include "src/core/board_hal.h"
#include "src/core/crash_log.h"
#include "src/video/camera_stream.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
// Optional diagnostic hook exported by the HomeTiles ESP-Hosted SDIO object.
// Keep it weak so stock-core and non-P4 builds remain link-compatible.
extern "C" void hometiles_sdio_get_rx_diag(
    uint32_t* last_pkt_len_raw, uint32_t* rx_byte_count,
    uint32_t* last_interrupts, uint32_t* legal_fffff_hits,
    uint32_t* pending_drains, uint32_t* alloc_retries,
    uint32_t* bus_faults) __attribute__((weak));
#endif

// Globale Instanz
HomeTilesNetworkManager networkManager;

static constexpr uint16_t kMqttBufferOta = 1024;
static constexpr uint16_t kMqttBufferNormal = 16 * 1024;
// Sobald Media-Tiles konfiguriert sind, muss der "normale" Puffer die
// Bridge-Media-States mit eingebettetem 240px-Cover fassen (~14 KB JPEG ->
// ~19 KB Base64+JSON). Mit 16 KB verwirft PubSubClient diese Pakete komplett;
// Cover kamen dann nur zufaellig waehrend eines 32-KB-Large-Fensters durch.
static constexpr uint16_t kMqttBufferMedia = 24 * 1024;
static constexpr uint16_t kMqttBufferLarge = 32 * 1024;
static constexpr uint32_t kMqttPostConnectQuietMs = 3000;
static constexpr uint32_t kWiredDhcpWaitMs = 10000;
// Solange der Ethernet-Link steht, WLAN erst nach dieser Frist ohne IP
// starten. Der hosted-Start frisst genau das DMA-RAM, das das Ethernet-
// Backend fuer seine Datenpuffer braucht - im Feldtest 2026-07-16 kam
// Ethernet deshalb nie hoch, waehrend WiFi gegen den toten C6 anrannte.
// Die Frist deckt zugleich Netze ohne DHCP ab (dann darf WiFi doch ran).
static constexpr uint32_t kWiredLinkWifiBlockMs = 60000;
// STA-Start-Fehlversuche in Folge, ab denen der ESP-Hosted-Treiber als
// tot gilt (RPC-Timeouts zum C6-Coprozessor).
static constexpr uint8_t kWifiStartWedgeThreshold = 3;
// Ein gesunder WiFi.begin()/Mode-Aufruf kehrt in Millisekunden zurueck. Nur
// wenn der C6 nicht mehr antwortet, laeuft er in den 5s-RPC-Timeout - dauert
// ein Verbindungsversuch laenger als diese Frist, zaehlt er als Wedge-Indiz
// (auch wenn er formal "erfolgreich" zurueckkehrt).
static constexpr uint32_t kWifiRpcSlowMs = 3000;
// WiFi.status() ist im ESP-Hosted-Stack gecacht. Wenn MQTT weg ist, obwohl
// dieser Cache weiter WL_CONNECTED meldet, nach kurzer Schonfrist ein echtes
// RPC pruefen. Bei gesundem WLAN kostet das nur Millisekunden; bei einem
// toten C6 entsteht genau ein 5s-Timeout, danach greift die sichere Recovery.
static constexpr uint32_t kWifiHealthProbeDelayMs = 15000;
static constexpr uint32_t kWifiHealthProbeIntervalMs = 60000;
// Automatischer Neustart wegen Wedge fruehestens nach dieser Laufzeit -
// verhindert eine enge Reboot-Schleife, falls der C6 dauerhaft defekt ist.
static constexpr uint32_t kWedgeRestartMinUptimeMs = 2 * 60 * 1000;
static constexpr uint8_t kMqttOutboundDrainNormal = 12;
static constexpr uint8_t kMqttOutboundDrainStorm = 1;
static constexpr size_t kMqttMinDmaLargestBeforeTx = 8 * 1024;
// Zusammenhaengender Notfallblock fuer ESP-Hosted. Er wird im gesunden
// Zustand absichtlich belegt gehalten und bei DMA-Druck freigegeben. Damit
// steht dem SDIO-RX/TX-Pfad sofort wieder ein unfragmentierter Block zur
// Verfuegung, statt darauf zu hoffen, dass viele kleine Heap-Luecken spaeter
// zufaellig zusammenwachsen.
static constexpr size_t kMqttDmaReserveBytes = 12 * 1024;
static constexpr size_t kMqttDmaReserveRearmLargest =
    kMqttDmaReserveBytes + kMqttMinDmaLargestBeforeTx + 4 * 1024;
static constexpr uint32_t kMqttDmaReserveRearmStableMs = 5000;
// Eine Large-Anfrage darf nie unbegrenzt an der DMA-Schwelle warten. Wenn die
// Reserve bereits freigegeben ist und der Heap sich trotzdem nicht erholt,
// wird nur der WLAN/SDIO-Transport kontrolliert neu aufgebaut.
static constexpr uint32_t kMqttDmaRecoveryWaitMs = 5000;
static constexpr uint32_t kMqttDmaRecoveryCooldownMs = 30000;
// Subscribe/Unsubscribe kann sofort ein retained Paket ausloesen. Auf dem P4
// bekommt der SDIO-RX-Task zwischen diesen Kontrollpaketen Zeit, das Paket bis
// in die MQTT-Inbound-Queue weiterzureichen und seinen DMA-Puffer freizugeben.
static constexpr uint32_t kMqttSdioControlQuietMs = 50;

static void applyWifiAutoReconnectPolicy() {
#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
  // ESP32-P4 uses ESP-Hosted control RPCs. HomeTiles owns normal reconnects;
  // this reduces repeated reconnect RPCs from the Arduino event task. Its
  // first retry is unconditional, so RPC serialization remains mandatory.
  WiFi.setAutoReconnect(false);
#else
  WiFi.setAutoReconnect(true);
#endif
}

// Waehrend dieses Fensters direkt nach dem Connect bleibt der MQTT-Empfangs-
// puffer klein. Er liegt inzwischen im PSRAM; das Ruhefenster verhindert aber
// weiterhin, dass eine grosse History-/Bridge-Antwort mit dem retained-
// Message-Sturm um die SDIO-RX-Puffer konkurriert.
static constexpr uint32_t kMqttStormWindowMs = 8000;

// ---------------------------------------------------------------------------
// Outbound-Command-Queues (Single-Owner MQTT)
//
// Gegenstueck zur Inbound-Queue in mqtt_handlers.cpp: jeder Task darf
// enqueuen, NUR der Worker-Task nimmt heraus und fasst mqtt_client an.
// Normale Publishes, Large-Response-Anfragen und SDIO-Kontrollkommandos liegen
// bewusst getrennt. History-/Energy-/Bridge-Anfragen muessen bei knapper
// DMA-Reserve warten, duerfen dabei aber niemals Szenen-, Licht- oder andere
// kleine Bedienbefehle hinter sich blockieren (Head-of-line-Blocking).
// Ein Allokations-Block pro Kommando, [MqttOutboundCmd][topic\0][payload],
// PSRAM bevorzugt -- 1:1 das Muster von mqttAllocInbound().
// ---------------------------------------------------------------------------
enum class MqttCmdKind : uint8_t { PUBLISH, SUBSCRIBE, UNSUBSCRIBE };

struct MqttOutboundCmd {
  MqttCmdKind kind;
  bool retain;
  uint32_t large_buffer_hold_ms;
  size_t payload_len;
  char* topic;       // -> in dieselbe Allokation
  uint8_t* payload;  // -> in dieselbe Allokation (leer bei SUBSCRIBE/UNSUBSCRIBE)
};

// 64 reichte rechnerisch fuer einen mqttReloadDynamicSlots()-Burst; der
// Post-Connect-Burst (kRoutes + Discovery + Settings + Snapshot + Reload,
// zusammen ~90 Kommandos) kann aber auflaufen, wenn der Worker gerade in
// einem grossen readPacket() steckt -- deshalb 128.
static constexpr size_t kMqttPublishQueueDepth = 128;
static constexpr size_t kMqttLargePublishQueueDepth = 32;
static constexpr size_t kMqttControlQueueDepth = 128;
static QueueHandle_t g_mqtt_publish_queue = nullptr;
static QueueHandle_t g_mqtt_large_publish_queue = nullptr;
static QueueHandle_t g_mqtt_control_queue = nullptr;
static uint32_t g_mqtt_outbound_dropped = 0;
static uint32_t g_mqtt_last_tx_guard_log_ms = 0;
static uint32_t g_mqtt_sdio_control_quiet_until = 0;
static void* g_mqtt_dma_reserve = nullptr;
static volatile bool g_mqtt_dma_reserve_held = false;
static uint32_t g_mqtt_dma_reserve_rearm_since = 0;
static uint32_t g_mqtt_dma_low_since = 0;
static uint32_t g_mqtt_dma_recovery_cooldown_until = 0;

static void initMqttDmaReserve() {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (g_mqtt_dma_reserve || !networkTransport.isSdioWifiActive()) return;
  g_mqtt_dma_reserve = heap_caps_malloc(
      kMqttDmaReserveBytes,
      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (g_mqtt_dma_reserve) {
    g_mqtt_dma_reserve_held = true;
    Serial.printf("[MQTT] DMA-Reserve angelegt: %u KB\n",
                  static_cast<unsigned>(kMqttDmaReserveBytes / 1024));
  } else {
    Serial.println("[MQTT] WARNUNG: DMA-Reserve konnte nicht angelegt werden");
  }
#endif
}

static void releaseMqttDmaReserve(const char* reason) {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (!g_mqtt_dma_reserve) return;
  g_mqtt_dma_reserve_held = false;
  heap_caps_free(g_mqtt_dma_reserve);
  g_mqtt_dma_reserve = nullptr;
  g_mqtt_dma_reserve_rearm_since = 0;
  Serial.printf("[MQTT] DMA-Reserve freigegeben (%s), largest=%u KB\n",
                reason ? reason : "?",
                static_cast<unsigned>(
                    heap_caps_get_largest_free_block(
                        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) /
                    1024));
#else
  (void)reason;
#endif
}

// Gibt bei Druck zuerst den garantierten zusammenhaengenden Reserveblock frei
// und legt ihn erst wieder an, wenn fuer mehrere Sekunden deutlich mehr als
// Reserve + Mindestblock verfuegbar war. So entsteht kein Alloc/Free-Pingpong.
static size_t serviceMqttDmaHeadroom(uint32_t now_ms) {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (!networkTransport.isSdioWifiActive()) {
    g_mqtt_dma_reserve_rearm_since = 0;
    return static_cast<size_t>(-1);
  }

  size_t largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (largest < kMqttMinDmaLargestBeforeTx && g_mqtt_dma_reserve) {
    const size_t largest_before_release = largest;
    g_mqtt_dma_reserve_held = false;
    heap_caps_free(g_mqtt_dma_reserve);
    g_mqtt_dma_reserve = nullptr;
    g_mqtt_dma_reserve_rearm_since = 0;
    largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    Serial.printf(
        "[MQTT] DMA-Reserve freigegeben: largest %u KB -> %u KB\n",
        static_cast<unsigned>(largest_before_release / 1024),
        static_cast<unsigned>(largest / 1024));
  }

  if (!g_mqtt_dma_reserve) {
    if (largest >= kMqttDmaReserveRearmLargest) {
      if (g_mqtt_dma_reserve_rearm_since == 0) {
        g_mqtt_dma_reserve_rearm_since = now_ms;
      } else if ((uint32_t)(now_ms - g_mqtt_dma_reserve_rearm_since) >=
                 kMqttDmaReserveRearmStableMs) {
        void* reserve = heap_caps_malloc(
            kMqttDmaReserveBytes,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (reserve) {
          g_mqtt_dma_reserve = reserve;
          g_mqtt_dma_reserve_held = true;
          largest = heap_caps_get_largest_free_block(
              MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
          Serial.printf("[MQTT] DMA-Reserve wieder angelegt, largest=%u KB\n",
                        static_cast<unsigned>(largest / 1024));
        }
        g_mqtt_dma_reserve_rearm_since = 0;
      }
    } else {
      g_mqtt_dma_reserve_rearm_since = 0;
    }
  }
  return largest;
#else
  (void)now_ms;
  return static_cast<size_t>(-1);
#endif
}

static MqttOutboundCmd* mqttAllocOutbound(MqttCmdKind kind,
                                          const char* topic,
                                          const uint8_t* payload,
                                          size_t payload_len,
                                          bool retain,
                                          uint32_t large_buffer_hold_ms) {
  if (!topic || !*topic) return nullptr;
  const size_t topic_len = strlen(topic);
  const size_t total = sizeof(MqttOutboundCmd) + topic_len + 1 + payload_len;
  uint8_t* block = static_cast<uint8_t*>(heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!block) block = static_cast<uint8_t*>(heap_caps_malloc(total, MALLOC_CAP_8BIT));
  if (!block) return nullptr;
  MqttOutboundCmd* cmd = reinterpret_cast<MqttOutboundCmd*>(block);
  cmd->kind = kind;
  cmd->retain = retain;
  cmd->large_buffer_hold_ms = large_buffer_hold_ms;
  cmd->payload_len = payload_len;
  cmd->topic = reinterpret_cast<char*>(block + sizeof(MqttOutboundCmd));
  cmd->payload = reinterpret_cast<uint8_t*>(cmd->topic + topic_len + 1);
  memcpy(cmd->topic, topic, topic_len);
  cmd->topic[topic_len] = '\0';
  if (payload_len) memcpy(cmd->payload, payload, payload_len);
  return cmd;
}

// Nie blockieren, nie inline verarbeiten (das wuerde mqtt_client vom
// falschen Task beruehren) -- bei voller Queue/Alloc-Fehler verwerfen+loggen.
static bool enqueueOutboundCmd(MqttCmdKind kind,
                               const char* topic,
                               const uint8_t* payload,
                               size_t payload_len,
                               bool retain,
                               bool priority = false,
                               uint32_t large_buffer_hold_ms = 0) {
  const bool large_publish =
      kind == MqttCmdKind::PUBLISH && large_buffer_hold_ms > 0;
  QueueHandle_t queue = kind == MqttCmdKind::PUBLISH
                            ? (large_publish ? g_mqtt_large_publish_queue
                                             : g_mqtt_publish_queue)
                            : g_mqtt_control_queue;
  if (!queue) return false;
  MqttOutboundCmd* cmd = mqttAllocOutbound(
      kind, topic, payload, payload_len, retain, large_buffer_hold_ms);
  if (!cmd) {
    Serial.println("[MQTT] Outbound-Alloc fehlgeschlagen -> Kommando verworfen");
    return false;
  }
  const BaseType_t queued = priority
                                ? xQueueSendToFront(queue, &cmd, 0)
                                : xQueueSend(queue, &cmd, 0);
  if (queued != pdTRUE) {
    heap_caps_free(cmd);
    ++g_mqtt_outbound_dropped;
    Serial.printf("[MQTT] Outbound-%s-Queue voll -> verworfen (#%u)\n",
                  kind != MqttCmdKind::PUBLISH
                      ? "Control"
                      : (large_publish ? "Large-Publish" : "Publish"),
                  static_cast<unsigned>(g_mqtt_outbound_dropped));
    return false;
  }
  return true;
}

static void purgeOutboundQueue() {
  QueueHandle_t queues[] = {g_mqtt_publish_queue,
                            g_mqtt_large_publish_queue,
                            g_mqtt_control_queue};
  for (QueueHandle_t queue : queues) {
    if (!queue) continue;
    MqttOutboundCmd* cmd = nullptr;
    while (xQueueReceive(queue, &cmd, 0) == pdTRUE) {
      if (cmd) heap_caps_free(cmd);
    }
  }
}

// Volle 48-Bit-MAC statt nur der unteren 16 Bit: zwei Geraete aus aehnlicher
// Fertigungscharge koennen in den unteren 16 Bit kollidieren (bei diesem
// Nutzer beobachtet -- zwei Panels meldeten dieselbe device_id und HA hat
// eines der beiden Zeroconf-Discovery-Events stillschweigend als "schon
// konfiguriert" verworfen). Kein "tab5_lvgl_"-Praefix mehr, nur die reine
// MAC als Hex-String.
void buildDeviceId(char* buffer, size_t len) {
  if (!buffer || !len) return;
  uint64_t mac = ESP.getEfuseMac();
  snprintf(buffer, len, "%012llX", (unsigned long long)(mac & 0xFFFFFFFFFFFFULL));
}

static void logMdnsHeap(const char* tag) {
  const uint32_t dma_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  Serial.printf("[mDNS] %s | DMA free=%u KB | DMA largest=%u KB\n",
                tag, dma_free / 1024, dma_largest / 1024);
}

static void logNetworkHeap(const char* tag) {
  const uint32_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const uint32_t dma_free =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t dma_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  Serial.printf("[Network/Mem] %s | Int free=%u KB | Int largest=%u KB | "
                "DMA free=%u KB | DMA largest=%u KB | PSRAM free=%u KB\n",
                tag ? tag : "?",
                static_cast<unsigned>(int_free / 1024),
                static_cast<unsigned>(int_largest / 1024),
                static_cast<unsigned>(dma_free / 1024),
                static_cast<unsigned>(dma_largest / 1024),
                static_cast<unsigned>(psram_free / 1024));
}

static bool parseConfiguredIp(const char* value, IPAddress& out) {
  if (!value || !value[0]) return false;
  String text = value;
  text.trim();
  if (!text.length()) return false;
  return out.fromString(text);
}

static void applyWifiAddressing(const DeviceConfig& cfg) {
  if (!cfg.wifi_static_enabled) {
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
    return;
  }

  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;

  const bool has_ip = parseConfiguredIp(cfg.wifi_static_ip, ip);
  const bool has_gateway = parseConfiguredIp(cfg.wifi_gateway, gateway);
  const bool has_subnet = parseConfiguredIp(cfg.wifi_subnet, subnet);
  const bool has_dns = parseConfiguredIp(cfg.wifi_dns, dns);

  if (has_ip || has_gateway || has_subnet || has_dns) {
    if (has_ip && has_gateway && has_subnet) {
      if (!has_dns) {
        dns = gateway;
      }
      if (WiFi.config(ip, gateway, subnet, dns)) {
        Serial.printf("WiFi: Static IP %s / GW %s / MASK %s / DNS %s\n",
                      ip.toString().c_str(),
                      gateway.toString().c_str(),
                      subnet.toString().c_str(),
                      dns.toString().c_str());
      } else {
        Serial.println("WiFi: Static IP configuration failed, fallback to DHCP");
        WiFi.config(IPAddress(), IPAddress(), IPAddress());
      }
    } else {
      Serial.println("WiFi: Incomplete static IP configuration, fallback to DHCP");
      WiFi.config(IPAddress(), IPAddress(), IPAddress());
    }
  } else {
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
  }
}

bool HomeTilesNetworkManager::isWiredConnected() const {
  return networkTransport.isUsbEthernetConnected() ||
         networkTransport.isNativeEthernetConnected();
}

bool HomeTilesNetworkManager::isWiredLinkUp() const {
  return networkTransport.isUsbEthernetLinkUp() ||
         networkTransport.isNativeEthernetLinkUp();
}

bool HomeTilesNetworkManager::isWifiStationEnabled() const {
  return (static_cast<uint8_t>(WiFi.getMode()) &
          static_cast<uint8_t>(WIFI_MODE_STA)) != 0;
}

bool HomeTilesNetworkManager::ensureWifiStationStarted() {
  wifi_suspended_for_wired = false;
  applyWifiAutoReconnectPolicy();
  if (!isWifiStationEnabled()) {
    if (!WiFi.mode(WIFI_STA)) {
      networkTransport.setWifiDriverActive(false);
      Serial.println("WiFi: STA-Start fehlgeschlagen");
      return false;
    }
    logNetworkHeap("after-WiFi.mode");
  }

  networkTransport.setWifiDriverActive(true);
  applyWifiAutoReconnectPolicy();
  WiFi.persistent(false);
  return true;
}

void HomeTilesNetworkManager::stopWifiForWired() {
  if (wifi_suspended_for_wired ||
      !networkTransport.isWifiDriverActive()) {
    return;
  }
  // Block every transport-level WiFi status/IP query before the hosted driver
  // teardown starts. On ESP32-P4, an overlapping RPC can otherwise outlive
  // the queue/semaphore it is using.
  wifi_suspended_for_wired = true;
  networkTransport.setWifiDriverActive(false);

  // ESP-Hosted and the SD card use separate P4 SDMMC slots, but IDF 5.5
  // deinitializes the shared SDMMC host queue when Hosted is torn down.
  // Unmount first and remount immediately afterwards so no stale semaphore
  // remains inside the mounted FAT driver.
  const bool sd_was_mounted = Device::suspendSDCardForNetworkTransition();
  WiFi.setAutoReconnect(false);
  // WIFI_OFF performs one controlled mode query followed by the complete
  // Arduino/ESP-Hosted teardown. Avoid the former disconnect()+getMode()
  // sequence, which issued several RPCs exactly while DMA was being released.
  const bool stopped = WiFi.mode(WIFI_OFF);
  if (sd_was_mounted && !Device::resumeSDCardAfterNetworkTransition()) {
    Serial.println("[Network] WARNUNG: SD-Karte nach WiFi-Stopp nicht gemountet");
  }
  networkTransport.setWifiDriverActive(!stopped);
  wifi_ps_state_known = false;

  if (stopped) {
    Serial.println("[Network] WiFi/SDIO gestoppt: Ethernet ist aktiv");
  } else {
    Serial.println("[Network] WiFi-Stopp fuer Ethernet fehlgeschlagen");
    wifi_suspended_for_wired = false;
  }
}

bool HomeTilesNetworkManager::recoverWifiFromDmaStarvation() {
  if (!mqtt_transport_recovery_requested) return false;

#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (!networkTransport.isSdioWifiActive() ||
      networkTransport.isEthernetMode()) {
    mqtt_transport_recovery_requested = false;
    mqtt_preserve_outbound_on_connect = false;
    return false;
  }

  Serial.println(
      "[Network] MQTT-TX-Recovery: WLAN/SDIO kontrolliert neu aufbauen");
  if (webAdminServer.isRunning()) webAdminServer.stop();
  stopMdns();
  was_connected = false;

  // Wie beim exklusiven Wechsel zu Ethernet zuerst SD sauber aushaengen:
  // ESP-Hosted und SD verwenden zwar verschiedene Slots, der IDF-Teardown
  // setzt aber die gemeinsame SDMMC-Host-Queue zurueck.
  const bool sd_was_mounted = Device::suspendSDCardForNetworkTransition();
  networkTransport.setWifiDriverActive(false);
  WiFi.setAutoReconnect(false);
  const bool stopped = WiFi.mode(WIFI_OFF);
  if (sd_was_mounted && !Device::resumeSDCardAfterNetworkTransition()) {
    Serial.println(
        "[Network] WARNUNG: SD-Karte nach WLAN-Recovery nicht gemountet");
  }

  wifi_ps_state_known = false;
  wifi_suspended_for_wired = false;
  mqtt_retry_at = 0;
  mqtt_connect_failures = 0;

  if (!stopped) {
    // Der alte Treiber ist noch aktiv. Den Worker wieder freigeben; sein
    // normaler Connect-Backoff bleibt funktionsfaehig und ein erneuter
    // Recovery-Versuch ist durch den Cooldown begrenzt.
    networkTransport.setWifiDriverActive(true);
    mqtt_transport_recovery_requested = false;
    Serial.println("[Network] WLAN/SDIO-Recovery: Treiber-Stopp fehlgeschlagen");
    return true;
  }

  // Dem IDF-Teardown Zeit geben, Tasks/Puffer vollstaendig freizugeben. Das
  // Display und sein schneller SRAM-Draw-Puffer bleiben dabei unangetastet.
  delay(100);
  wifi_retry_at = 0;
  connectWifi();
  mqtt_transport_recovery_requested = false;
  Serial.println("[Network] WLAN/SDIO-Recovery: Neuaufbau angestossen");
  return true;
#else
  mqtt_transport_recovery_requested = false;
  mqtt_preserve_outbound_on_connect = false;
  return false;
#endif
}

// ========== Initialisierung ==========
void HomeTilesNetworkManager::init() {
  networkTransport.begin();
  networkTransport.update();
  transport_generation_seen = networkTransport.generation();
  Serial.println("🌐 Initialisiere Network Manager...");

  if (!configManager.isConfigured()) {
    Serial.println("⚠️ Keine Netzwerk-Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  wired_was_connected = isWiredConnected();
  wired_link_was_up = isWiredLinkUp();
  if (wired_link_was_up && !wired_was_connected) {
    wired_ip_wait_until = millis() + kWiredDhcpWaitMs;
  }
  if (networkTransport.isEthernetMode()) {
    // Fester Ethernet-Modus: WLAN/ESP-Hosted startet in dieser Boot-Session
    // NIE - ohne Kabel/Adapter ist das Geraet bewusst offline statt in den
    // frueheren WiFi-Fallback zu laufen, der beide Stacks gleichzeitig ins
    // DMA-RAM gezwungen hat.
    Serial.printf("[Network] Ethernet-Modus: %s, WLAN bleibt aus\n",
                  wired_was_connected ? networkTransport.activeName()
                                      : "warte auf Link/DHCP");
  } else {
    // WLAN-Modus: kein Ethernet-Backend gestartet, also entfaellt auch die
    // fruehere Wartefrist vor dem WiFi-Start.
    ensureWifiStationStarted();
    wifi_retry_at = 0;  // Sofortiger Verbindungsversuch
  }

  // Bridge-/Request-Topics EINMALIG hier bauen: sie haengen nur von der
  // (laufzeit-konstanten) Efuse-MAC ab. Frueher baute connectMqtt() sie bei
  // jedem Reconnect neu -- sobald connectMqtt() auf dem Worker laeuft,
  // waehrend der Loop-Task getBridgeApplyTopic() etc. liest, waere jedes
  // String-Reassignment ein echtes Race. init() laeuft vor dem Worker-Start.
  char did[24];
  buildDeviceId(did, sizeof(did));
  String base = "tab5_lvgl/config/";
  base += did;
  bridge_apply_topic_ = base + "/bridge/apply";
  bridge_request_topic_ = base + "/bridge/request";
  history_request_topic_ = base + "/history/request";
  history_response_topic_ = base + "/history/response";
  weather_request_topic_ = base + "/weather/request";
  energy_request_topic_ = base + "/energy/request";
  energy_response_topic_ = base + "/energy/response";
  bridge_icons_topic_ = base + "/bridge/icons";

  mqtt_enabled = configManager.hasMqttConfig();
  if (mqtt_enabled) {
    // MQTT-Setup (vor Worker-Start, daher direkter Client-Zugriff okay)
    mqtt_client.setClient(net_client);
    mqtt_client.setServer(cfg.mqtt_host, cfg.mqtt_port);
    setMqttBufferSize(mqttNormalBufferSize(), "init");
    mqtt_client.setCallback(mqttCallback);
  } else {
    Serial.println("MQTT: keine Konfiguration vorhanden - ueberspringe Verbindung");
  }

  Serial.println("✓ Network Manager initialisiert");
}

// ========== WiFi verbinden ==========
void HomeTilesNetworkManager::connectWifi() {
  wifi_retry_at = millis() + 5000UL;  // Retry in 5s

  // Fester Ethernet-Modus: WLAN bleibt aus, egal was Retry-/Reconnect-Logik
  // oder UI-Aufrufer wollen. Zurueck zu WLAN geht nur ueber den
  // Netzwerkmodus-Schalter + Neustart.
  if (networkTransport.isEthernetMode()) return;

  // Treiber als tot markiert (C6 antwortet nicht): keine weiteren Versuche.
  // Jeder Aufruf in den halbtoten hosted-Stack blockiert den Loop-Task fuer
  // Sekunden (RPC-Timeout) - den Ausweg regelt update() (Ethernet/Neustart).
  if (wifi_wedge_latched) return;

  // Jeder Verbindungsaufbau (manuell, neue Zugangsdaten, AP-Ende) hebt ein
  // vorheriges manuelles Trennen wieder auf.
  if (wifi_manual_disconnect) {
    wifi_manual_disconnect = false;
    applyWifiAutoReconnectPolicy();
  }

  networkTransport.update();
  if (isWiredConnected()) {
    stopWifiForWired();
    Serial.printf("WiFi: nicht gestartet, %s ist aktiv\n",
                  networkTransport.activeName());
    return;
  }
  if (isWiredLinkUp()) {
    // Solange der Kabel-Link steht, gehoert das DMA-RAM dem Ethernet-Backend
    // (Puffer-Allokation + DHCP-Anlauf). WiFi darf erst uebernehmen, wenn
    // Ethernet nach kWiredLinkWifiBlockMs immer noch keine IP hat (z.B. Netz
    // ohne DHCP) - nicht schon nach dem kurzen DHCP-Fenster.
    const uint32_t link_since = wired_link_up_since;
    if (link_since == 0 ||
        (uint32_t)(millis() - link_since) < kWiredLinkWifiBlockMs) {
      Serial.println("WiFi: nicht gestartet, Ethernet-Link aktiv");
      return;
    }
  }

  if (!configManager.isConfigured()) {
    Serial.println("WiFi: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();
  if (cfg.wifi_ssid && cfg.wifi_ssid[0]) {
    const uint32_t attempt_started = millis();
    if (!ensureWifiStationStarted()) {
      // STA-Start fehlgeschlagen = RPC zum C6 tot oder Speicher am Limit.
      // Exponentiell zurueckziehen statt im 5s-Takt je ~5s RPC-Timeout auf
      // dem Loop-Task zu verbrennen; ab der Schwelle den Wedge behandeln.
      if (wifi_start_failures < 255) ++wifi_start_failures;
      const uint32_t shift =
          wifi_start_failures < 4 ? wifi_start_failures : 4;
      wifi_retry_at = millis() + (5000UL << shift);  // 10s..80s
      if (wifi_start_failures >= kWifiStartWedgeThreshold) {
        handleWifiDriverWedge();
      }
      return;
    }
    Serial.printf("WiFi: Verbinde mit %s\n", cfg.wifi_ssid);
    applyWifiAddressing(cfg);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

    // Wedge-Indiz auch OHNE formalen Fehler: Gesund kehren diese Aufrufe in
    // Millisekunden zurueck; nur ein toter C6 laesst sie in die 5s-RPC-
    // Timeouts laufen. Erst der naechste erfolgreich SCHNELLE Versuch (oder
    // eine stehende Verbindung, siehe update()) setzt den Zaehler zurueck.
    const uint32_t attempt_ms = millis() - attempt_started;
    if (attempt_ms >= kWifiRpcSlowMs) {
      if (wifi_start_failures < 255) ++wifi_start_failures;
      Serial.printf(
          "WiFi: Verbindungsversuch blockierte %lu ms (RPC-Timeout-Verdacht %u/%u)\n",
          static_cast<unsigned long>(attempt_ms),
          static_cast<unsigned>(wifi_start_failures),
          static_cast<unsigned>(kWifiStartWedgeThreshold));
      const uint32_t shift =
          wifi_start_failures < 4 ? wifi_start_failures : 4;
      wifi_retry_at = millis() + (5000UL << shift);
      if (wifi_start_failures >= kWifiStartWedgeThreshold) {
        handleWifiDriverWedge();
      }
    } else {
      wifi_start_failures = 0;
    }
  }
}

bool HomeTilesNetworkManager::probeWifiDriverHealth(const char* context) {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (wifi_wedge_latched ||
      networkTransport.isEthernetMode() ||
      !networkTransport.isWifiDriverActive()) {
    return !wifi_wedge_latched;
  }

  const uint32_t started = millis();
  (void)WiFi.getMode();
  const uint32_t elapsed = millis() - started;
  if (elapsed < kWifiRpcSlowMs) return true;

  wifi_start_failures = kWifiStartWedgeThreshold;
  Serial.printf(
      "[Network] WLAN-Liveness-Probe blockierte %lu ms (%s): "
      "ESP-Hosted/C6 antwortet nicht\n",
      static_cast<unsigned long>(elapsed),
      context && context[0] ? context : "ohne Kontext");
  handleWifiDriverWedge(context);
  return false;
#else
  (void)context;
  return true;
#endif
}

void HomeTilesNetworkManager::handleWifiDriverWedge(const char* context) {
  if (wifi_wedge_latched) return;
  wifi_wedge_latched = true;

  const bool wired = isWiredLinkUp();
  String detail;
  detail.reserve(512);
  detail += "ESP-Hosted RPC-Timeout zum C6";
  if (context && context[0]) {
    detail += " (";
    detail += context;
    detail += ')';
  }
  detail += '\n';
  detail += "Wedge-Indizien ";
  detail += wifi_start_failures;
  detail += '/';
  detail += kWifiStartWedgeThreshold;
  detail += '\n';
  detail += "Uptime ";
  detail += millis() / 1000;
  detail += " s | Ethernet-Link ";
  detail += wired ? "vorhanden" : "fehlt";
  detail += '\n';
  char mem[96];
  snprintf(mem, sizeof(mem), "mem int=%uKB largest=%uKB dma_largest=%uKB\n",
           static_cast<unsigned>(
               heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           static_cast<unsigned>(
               heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
           static_cast<unsigned>(heap_caps_get_largest_free_block(
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) /
                                 1024));
  detail += mem;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (hometiles_sdio_get_rx_diag) {
    uint32_t last_raw = 0;
    uint32_t consumed = 0;
    uint32_t last_intr = 0;
    uint32_t legal_fffff = 0;
    uint32_t pending_drains = 0;
    uint32_t alloc_retries = 0;
    uint32_t bus_faults = 0;
    hometiles_sdio_get_rx_diag(
        &last_raw, &consumed, &last_intr, &legal_fffff,
        &pending_drains, &alloc_retries, &bus_faults);
    char sdio_diag[224];
    snprintf(
        sdio_diag, sizeof(sdio_diag),
        "sdio raw=0x%08lX consumed=%lu intr=0x%08lX "
        "legal_fffff=%lu pending_drains=%lu alloc_retries=%lu "
        "bus_faults=%lu\n",
        static_cast<unsigned long>(last_raw),
        static_cast<unsigned long>(consumed),
        static_cast<unsigned long>(last_intr),
        static_cast<unsigned long>(legal_fffff),
        static_cast<unsigned long>(pending_drains),
        static_cast<unsigned long>(alloc_retries),
        static_cast<unsigned long>(bus_faults));
    detail += sdio_diag;
    Serial.printf("[Network] %s", sdio_diag);
  } else {
    detail += "sdio diagnostics unavailable (stock/old hosted object)\n";
  }
#endif
  detail += camera_stream_is_active()
                ? "camera_stream=active\n"
                : "camera_stream=inactive\n";
  detail += wired
                ? "Weiterlauf ueber Ethernet, WiFi bis zum Neustart deaktiviert\n"
                : "Sicherer Neustart folgt (setzt den C6 mit zurueck)\n";
  CrashLog::appendNetworkWedgeReport(detail);

  Serial.printf("[Network] WLAN-Treiber reagiert nicht mehr - %s\n",
                wired ? "laufe auf Ethernet weiter (WiFi aus bis Neustart)"
                      : "sicherer Neustart");
  if (!wired) {
    if (millis() >= kWedgeRestartMinUptimeMs) {
      Serial.flush();
      BoardHAL::prepareForRestart();
      delay(500);
      BoardHAL::restart();
    }
    // Zu frueh fuer einen Auto-Neustart: latched lassen; update() startet
    // neu, sobald die Mindestlaufzeit erreicht ist und kein Link da ist.
  }
}

// ========== WiFi manuell trennen (WLAN-Popup "Trennen") ==========
void HomeTilesNetworkManager::disconnectWifiManual() {
  wifi_manual_disconnect = true;
  if (networkTransport.activeKind() == NetworkTransportKind::Wifi &&
      isMqttConnected()) {
    disconnectMqtt();
  }
  WiFi.setAutoReconnect(false);
  if (isWifiStationEnabled()) WiFi.disconnect();
  Serial.println("WiFi: manuell getrennt (kein Auto-Reconnect bis Verbinden/Neustart)");
}

// ========== MQTT verbinden (worker-only) ==========
void HomeTilesNetworkManager::connectMqtt() {
  if (!mqtt_enabled) return;
  mqtt_retry_at = millis() + 3000UL;  // Retry in 3s

  if (!networkTransport.isConnected()) return;
  if (mqtt_large_until == 0 && mqtt_client.getBufferSize() < mqttNormalBufferSize()) {
    setMqttBufferSize(mqttNormalBufferSize(), "connect");
  }

  if (!configManager.isConfigured()) {
    Serial.println("MQTT: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  char client_id[CONFIG_MQTT_CLIENT_ID_MAX];
  if (cfg.mqtt_client_id[0]) {
    snprintf(client_id, sizeof(client_id), "%s", cfg.mqtt_client_id);
  } else {
    const unsigned long long mac = static_cast<unsigned long long>(ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL);
    snprintf(client_id, sizeof(client_id), "Tab5_LVGL-%012llX", mac);
  }

  Serial.printf("MQTT: Verbinde mit %s:%u als %s\n", cfg.mqtt_host, cfg.mqtt_port, client_id);

  const char* stat_topic = mqttTopics.topic(TopicKey::STAT_CONN);
  if (!stat_topic || !*stat_topic) {
    stat_topic = "tab5/stat/connected";
  }

  bool ok = false;
  if (cfg.mqtt_user && cfg.mqtt_user[0]) {
    ok = mqtt_client.connect(client_id, cfg.mqtt_user, cfg.mqtt_pass,
                             stat_topic, 0, true, "0");
  } else {
    ok = mqtt_client.connect(client_id, nullptr, nullptr,
                             stat_topic, 0, true, "0");
  }

  if (!ok) {
    // Exponentiell zurueckziehen: Ein toter Broker oder ein gewedgter WLAN-
    // Stack wird sonst im 3s-Takt mit blockierenden Connects gehammert, die
    // den Loop-/Worker-Takt und das interne RAM auffressen (Feldtest
    // 2026-07-16: Endlos-State=-2-Schleife bis 33 KB Heap-Minimum).
    if (mqtt_connect_failures < 255) ++mqtt_connect_failures;
    const uint32_t shift =
        mqtt_connect_failures < 5 ? mqtt_connect_failures : 5;
    mqtt_retry_at = millis() + (3000UL << shift);  // 6s..96s
    Serial.printf("MQTT: Verbindung fehlgeschlagen, State=%d (Retry in %lus)\n",
                  mqtt_client.state(),
                  static_cast<unsigned long>((3000UL << shift) / 1000));
    return;
  }

  mqtt_connect_failures = 0;
  Serial.println("✓ MQTT verbunden");
  mqtt_connected_at = millis();
  logNetworkHeap("after-MQTT-connect");

  // Status publizieren und die Antwort-Topics direkt subscriben -- direkter
  // Client-Zugriff ist hier safe, weil connectMqtt() ausschliesslich auf dem
  // Worker laeuft (Single-Owner).
  mqtt_client.publish(stat_topic, "1", true);
  const char* ip_topic = mqttTopics.topic(TopicKey::STAT_IP);
  if (ip_topic && *ip_topic) {
    mqtt_client.publish(
        ip_topic, networkTransport.localIP().toString().c_str(), true);
  }
  if (!bridge_apply_topic_.isEmpty()) {
    if (mqtt_client.subscribe(bridge_apply_topic_.c_str())) {
      Serial.printf("[MQTT] Listening for bridge config on %s\n",
                    bridge_apply_topic_.c_str());
    } else {
      Serial.printf("[MQTT] ERROR subscribing to bridge config on %s\n",
                    bridge_apply_topic_.c_str());
    }
  }
  if (!history_response_topic_.isEmpty()) {
    mqtt_client.subscribe(history_response_topic_.c_str());
    Serial.printf("[MQTT] Listening for history responses on %s\n", history_response_topic_.c_str());
  }
  if (!energy_response_topic_.isEmpty()) {
    mqtt_client.subscribe(energy_response_topic_.c_str());
    Serial.printf("[MQTT] Listening for energy responses on %s\n", energy_response_topic_.c_str());
  }
  if (!bridge_icons_topic_.isEmpty()) {
    mqtt_client.subscribe(bridge_icons_topic_.c_str());
    Serial.printf("[MQTT] Listening for icon updates on %s\n", bridge_icons_topic_.c_str());
  }

  // Nach einer normalen Unterbrechung sind alte Bedienkommandos nicht mehr
  // aktuell. Beim gezielten DMA-Recovery bleiben die bereits angenommenen
  // Requests dagegen erhalten: sie waren nie auf dem Broker und werden direkt
  // nach dem Wiederaufbau ueber die bereits oben abonnierten Antwort-Topics
  // abgearbeitet.
  if (mqtt_preserve_outbound_on_connect) {
    mqtt_preserve_outbound_on_connect = false;
    Serial.println("[MQTT] DMA-Recovery: wartende Requests bleiben erhalten");
  } else {
    purgeOutboundQueue();
  }

  mqtt_connected_flag = true;

  // Die App-Ebene (mqttSubscribeTopics/Discovery/DeviceSettings/Snapshot)
  // faehrt der LOOP-Task hoch (mqttServicePostConnect): diese Funktionen
  // scannen Flash, pumpen LVGL und lesen Batterie-I2C -- nichts davon darf
  // auf dem Worker laufen. Ihre publishes/subscribes kommen per
  // Outbound-Queue hierher zurueck.
  mqtt_post_connect_ready_at = mqtt_connected_at + kMqttPostConnectQuietMs;
  mqtt_post_connect_pending = true;

  // Bridge requests are handled by the background refresh. The retained
  // device announcement is queued later by mqttServicePostConnect(); its
  // large-buffer lane waits until the startup RAM storm has passed.
}

// ========== Single-Owner MQTT: Worker ==========
void HomeTilesNetworkManager::beginMqttWorker() {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  Serial.println(
      "[Network] ESP-Hosted SDIO-Puffer: DMA-PSRAM bevorzugt, "
      "interner DMA-Fallback aktiv");
#endif
  if (!g_mqtt_publish_queue) {
    g_mqtt_publish_queue =
        xQueueCreate(kMqttPublishQueueDepth, sizeof(MqttOutboundCmd*));
    if (!g_mqtt_publish_queue) {
      Serial.println("[MQTT] Outbound-Publish-Queue konnte nicht erstellt werden");
    }
  }
  if (!g_mqtt_large_publish_queue) {
    g_mqtt_large_publish_queue =
        xQueueCreate(kMqttLargePublishQueueDepth, sizeof(MqttOutboundCmd*));
    if (!g_mqtt_large_publish_queue) {
      Serial.println(
          "[MQTT] Outbound-Large-Publish-Queue konnte nicht erstellt werden");
    }
  }
  if (!g_mqtt_control_queue) {
    g_mqtt_control_queue =
        xQueueCreate(kMqttControlQueueDepth, sizeof(MqttOutboundCmd*));
    if (!g_mqtt_control_queue) {
      Serial.println("[MQTT] Outbound-Control-Queue konnte nicht erstellt werden");
    }
  }
  initMqttDmaReserve();
}

size_t HomeTilesNetworkManager::mqttDmaReserveBytes() const {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  return g_mqtt_dma_reserve_held ? kMqttDmaReserveBytes : 0;
#else
  return 0;
#endif
}

// Worker-Task-Body: die EINZIGE Stelle, die mqtt_client nach init() anfasst.
void HomeTilesNetworkManager::serviceMqttWorker() {
  // Reconfigure-Request zuerst und VOR dem mqtt_enabled-Gate geprueft: genau
  // dieses Flag soll hier live neu gesetzt werden (Erstkonfiguration ueber
  // die Admin-Seite, Host geleert, Host geaendert). Alle anderen Requests
  // unten bleiben bewusst hinter dem Gate, die betreffen nur ein Geraet, das
  // bereits mqtt_enabled war.
  if (mqtt_reconfig_requested) {
    mqtt_reconfig_requested = false;
    if (mqtt_client.connected()) {
      const char* stat_topic = mqttTopics.topic(TopicKey::STAT_CONN);
      if (stat_topic && *stat_topic) {
        // Sauberes "0" vor dem Disconnect -- ein regulaeres MQTT DISCONNECT
        // (was PubSubClient::disconnect() sendet) loest das Last-Will NICHT
        // aus, die Bridge wuerde also faelschlich "verbunden" weiterzeigen.
        mqtt_client.publish(stat_topic, "0", true);
      }
      mqtt_client.disconnect();
      Serial.println("[MQTT] Disconnect fuer Reconfigure");
    }
    purgeOutboundQueue();
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    mqtt_large_until = 0;
    if (mqtt_buffer_size > mqttNormalBufferSize()) {
      setMqttBufferSize(mqttNormalBufferSize(), "reconfig");
    }
    mqtt_connected_flag = false;

    mqtt_enabled = configManager.hasMqttConfig();
    if (mqtt_enabled) {
      const DeviceConfig& cfg = configManager.getConfig();
      mqtt_client.setClient(net_client);
      mqtt_client.setServer(cfg.mqtt_host, cfg.mqtt_port);
      mqtt_client.setCallback(mqttCallback);
      mqtt_retry_at = 0;  // sofortiger Verbindungsversuch, naechste Iteration
      mqtt_connect_failures = 0;  // frischer Transport, frischer Backoff
      Serial.println("[MQTT] Reconfigure: neue Einstellungen uebernommen");
    } else {
      Serial.println("[MQTT] Reconfigure: kein Host konfiguriert, bleibe getrennt");
    }
    return;
  }

  if (mqtt_transport_recovery_requested) return;
  if (!mqtt_enabled) return;

  // Request-Flags zuerst -- auch im suspendierten Zustand, damit
  // restoreMqttBufferNormal() den Worker nach einem abgebrochenen OTA wieder
  // aufwecken kann.
  if (mqtt_ota_prep_requested) {
    mqtt_large_until = 0;
    if (mqtt_client.connected()) {
      mqtt_client.disconnect();
      Serial.println("[OTA] MQTT disconnected for OTA");
    }
    setMqttBufferSize(kMqttBufferOta, "ota");
    // Der Block schuetzt ausschliesslich MQTT-Publishes. Waehrend OTA ist der
    // Worker suspendiert; die zusammenhaengenden 12 KB gehoeren deshalb dem
    // ESP-Hosted-/HTTP-RX-Pfad und werden nach der Wiederherstellung
    // automatisch erneut angelegt.
    releaseMqttDmaReserve("ota");
    mqtt_connected_flag = false;
    mqtt_suspended = true;  // waehrend OTA weder reconnecten noch loop() pumpen
    mqtt_ota_prep_requested = false;
    return;
  }
  if (mqtt_restore_normal_requested) {
    mqtt_restore_normal_requested = false;
    mqtt_large_until = 0;
    setMqttBufferSize(mqttNormalBufferSize(), "normal");
    mqtt_suspended = false;  // nach abgebrochenem OTA weitermachen
    return;
  }
  if (mqtt_disconnect_requested) {
    if (mqtt_client.connected()) {
      mqtt_client.disconnect();
      Serial.println("[MQTT] Disconnect auf Anforderung (Hotspot-Modus)");
    }
    purgeOutboundQueue();
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    mqtt_large_until = 0;
    if (mqtt_buffer_size > mqttNormalBufferSize()) {
      setMqttBufferSize(mqttNormalBufferSize(), "disconnect");
    }
    mqtt_connected_flag = false;
    mqtt_disconnect_requested = false;
    return;
  }
  if (mqtt_suspended) return;

  if (!networkTransport.isConnected()) {
    if (mqtt_connected_flag) mqtt_connected_flag = false;
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    return;
  }

  const uint32_t now_ms = millis();
  const uint32_t hold_until = mqtt_reconnect_hold_until;
  if (hold_until != 0 && (int32_t)(now_ms - hold_until) < 0) {
    if (mqtt_client.connected()) {
      mqtt_client.disconnect();
      Serial.println("[MQTT] Disconnect waehrend Reconnect-Ruhefenster");
    }
    if (mqtt_connected_flag) mqtt_connected_flag = false;
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    return;
  }
  if (hold_until != 0) {
    mqtt_reconnect_hold_until = 0;
  }

  serviceBufferHousekeeping(now_ms);

  if (!mqtt_client.connected()) {
    if (mqtt_connected_flag) {
      mqtt_connected_flag = false;
      Serial.println("[MQTT] Verbindung verloren");
    }
    if ((int32_t)(now_ms - mqtt_retry_at) >= 0) {
      connectMqtt();
    }
    return;
  }

  // Erst ausgehende Kommandos, dann den Socket pumpen. Large-Buffer-Wuensche
  // reisen mit dem Publish-Kommando und werden im Drain unmittelbar vor dem
  // Versand aktiviert.
  const bool startup_storm =
      mqtt_connected_at != 0 && (uint32_t)(now_ms - mqtt_connected_at) < kMqttStormWindowMs;
  drainOutboundQueues(startup_storm ? kMqttOutboundDrainStorm
                                    : kMqttOutboundDrainNormal);
  mqtt_client.loop();
  if (!mqtt_client.connected()) {
    Serial.printf("[MQTT] Verbindung in loop verloren, State=%d\n",
                  mqtt_client.state());
    mqtt_connected_flag = false;
  }
}

void HomeTilesNetworkManager::drainOutboundQueues(uint8_t max_commands) {
  if (max_commands == 0) return;

  const uint32_t now_ms = millis();
  size_t dma_largest = static_cast<size_t>(-1);
  bool control_quiet = false;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (networkTransport.isSdioWifiActive()) {
    control_quiet =
        g_mqtt_sdio_control_quiet_until != 0 &&
        static_cast<int32_t>(now_ms - g_mqtt_sdio_control_quiet_until) < 0;
    if (!control_quiet) g_mqtt_sdio_control_quiet_until = 0;
    // Auch ohne wartende Large-Anfrage die Reserve bei akutem Druck sofort
    // freigeben. So bekommt bereits der laufende SDIO-RX-Pfad Headroom.
    dma_largest = serviceMqttDmaHeadroom(now_ms);
  }
#endif

  auto log_dma_wait = [&](const char* lane, size_t largest) {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if ((uint32_t)(now_ms - g_mqtt_last_tx_guard_log_ms) >= 2000) {
      g_mqtt_last_tx_guard_log_ms = now_ms;
      Serial.printf("[MQTT] %s wartet: DMA largest nur %u KB\n",
                    lane,
                    static_cast<unsigned>(largest / 1024));
    }
#else
    (void)lane;
    (void)largest;
#endif
  };

  // Kontrollkommandos behalten Prioritaet, werden auf dem P4 aber weiterhin
  // einzeln und mit Abstand gesendet. Waehrend des Schutzabstands laufen
  // Publishes aus ihrer getrennten Queue trotzdem weiter.
  MqttOutboundCmd* cmd = nullptr;
  const bool control_waiting =
      g_mqtt_control_queue &&
      xQueuePeek(g_mqtt_control_queue, &cmd, 0) == pdTRUE;
  if (control_waiting && !control_quiet) {
    if (xQueueReceive(g_mqtt_control_queue, &cmd, 0) == pdTRUE) {
      bool ok = false;
      const char* verb = "control";
      if (cmd) {
        if (cmd->kind == MqttCmdKind::SUBSCRIBE) {
          verb = "subscribe";
          ok = mqtt_client.subscribe(cmd->topic);
        } else if (cmd->kind == MqttCmdKind::UNSUBSCRIBE) {
          verb = "unsubscribe";
          ok = mqtt_client.unsubscribe(cmd->topic);
        }
        if (!ok) {
          Serial.printf("[MQTT] Worker: %s '%s' fehlgeschlagen\n",
                        verb, cmd->topic);
        }
        heap_caps_free(cmd);
      }
#if defined(CONFIG_IDF_TARGET_ESP32P4)
      if (networkTransport.isSdioWifiActive()) {
        g_mqtt_sdio_control_quiet_until = millis() + kMqttSdioControlQuietMs;
      }
#endif
      return;
    }
  }

  // Direkt nach einem Subscribe/Unsubscribe hoechstens ein Publish, danach
  // wird mqtt_client.loop() aufgerufen und kann ein sofortiges retained Paket
  // abholen. So bleibt der bisherige SDIO-Schutz erhalten, ohne die Publish-
  // Lane fuer die gesamten 50 ms komplett anzuhalten.
  const uint8_t publish_limit = control_quiet ? 1 : max_commands;
  const bool startup_storm =
      mqtt_connected_at != 0 &&
      (uint32_t)(millis() - mqtt_connected_at) < kMqttStormWindowMs;
  MqttOutboundCmd* large_peek = nullptr;
  const bool large_waiting =
      !startup_storm && g_mqtt_large_publish_queue &&
      xQueuePeek(g_mqtt_large_publish_queue, &large_peek, 0) == pdTRUE;
  // Ausserhalb des Startsturms bleibt bei einem normalen 12er-Drain immer ein
  // Platz fuer die Large-Lane. Ein dauernder Strom kleiner Statuspublishes darf
  // History/Energy/Bridge nicht verhungern lassen.
  const uint8_t normal_limit =
      large_waiting && publish_limit > 1 ? publish_limit - 1 : publish_limit;
  uint32_t drained = 0;
  if (g_mqtt_publish_queue) {
    while (drained < normal_limit &&
           xQueueReceive(g_mqtt_publish_queue, &cmd, 0) == pdTRUE) {
      if (!cmd) continue;

      // Normale, kleine Bedienpublishes brauchen keinen 32-KB-Empfangspuffer
      // und bleiben auch bei fragmentiertem DMA-RAM sendefaehig. Genau diese
      // Lane darf durch wartende History-/Energy-/Bridge-Anfragen niemals
      // angehalten werden.
      const bool ok = mqtt_client.publish(
          cmd->topic, cmd->payload, cmd->payload_len, cmd->retain);
      if (!ok) {
        Serial.printf("[MQTT] Worker: publish '%s' fehlgeschlagen\n", cmd->topic);
      }
      heap_caps_free(cmd);

      // Auch ein grosser Publish-Burst darf den Idle-Task nicht aushungern.
      if ((++drained & 0x07) == 0) vTaskDelay(1);
    }
  }

  if (!large_waiting || drained >= publish_limit) {
    g_mqtt_dma_low_since = 0;
    return;
  }

  // Large-Response-Anfragen erst nach dem Retained-Sturm senden. Sie bleiben
  // waehrenddessen in ihrer eigenen Queue; normale Bedienbefehle oben laufen
  // trotzdem weiter.
  while (drained < publish_limit &&
         xQueuePeek(g_mqtt_large_publish_queue, &cmd, 0) == pdTRUE) {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // Die DMA-Pruefung MUSS vor der Puffervergroesserung stehen. Bei zu
    // kleiner Reserve bleibt nur die Large-Lane stehen; die normale Lane
    // wurde oben bereits abgearbeitet und bleibt damit immer bedienbar.
    if (networkTransport.isSdioWifiActive()) {
      dma_largest = serviceMqttDmaHeadroom(now_ms);
      if (dma_largest < kMqttMinDmaLargestBeforeTx) {
        log_dma_wait("Large-Publish", dma_largest);
        if (g_mqtt_dma_low_since == 0) {
          g_mqtt_dma_low_since = now_ms;
        }
        const bool recovery_allowed =
            g_mqtt_dma_recovery_cooldown_until == 0 ||
            static_cast<int32_t>(
                now_ms - g_mqtt_dma_recovery_cooldown_until) >= 0;
        if (recovery_allowed &&
            (uint32_t)(now_ms - g_mqtt_dma_low_since) >=
                kMqttDmaRecoveryWaitMs) {
          // Never tear down ESP-Hosted while the camera task owns an active
          // HTTP socket. The teardown invalidates that socket underneath the
          // other core and caused an instruction-access panic. The camera
          // path has its own DMA guard and will close first if memory remains
          // scarce; normal MQTT publishes continue through their own lane.
          if (camera_stream_is_active()) {
            log_dma_wait("Recovery wartet auf Kamera-Ende", dma_largest);
            return;
          }
          Serial.printf(
              "[MQTT] DMA-Starvation seit %u ms: WLAN/SDIO-Recovery\n",
              static_cast<unsigned>(now_ms - g_mqtt_dma_low_since));
          // Single-Owner-Regel bleibt erhalten: der Worker schliesst seinen
          // Client selbst, erst danach darf der Loop-Task den Treiber abbauen.
          if (mqtt_client.connected()) mqtt_client.disconnect();
          mqtt_connected_flag = false;
          mqtt_post_connect_pending = false;
          mqtt_post_connect_ready_at = 0;
          mqtt_large_until = 0;
          mqtt_preserve_outbound_on_connect = true;
          g_mqtt_dma_low_since = 0;
          g_mqtt_dma_recovery_cooldown_until =
              now_ms + kMqttDmaRecoveryCooldownMs;
          mqtt_transport_recovery_requested = true;
        }
        return;
      }
    }
#endif
    g_mqtt_dma_low_since = 0;

    cmd = nullptr;
    if (xQueueReceive(g_mqtt_large_publish_queue, &cmd, 0) != pdTRUE) return;
    if (!cmd) continue;

    // Der grosse Puffer wird erst jetzt angelegt, nachdem die DMA-Reserve
    // ausreicht und das Kommando unmittelbar vor dem Versand steht.
    if (mqtt_buffer_size < kMqttBufferLarge &&
        !setMqttBufferSize(kMqttBufferLarge, "queued-publish")) {
      ++g_mqtt_outbound_dropped;
      Serial.printf(
          "[MQTT] Large-Publish verworfen: Puffer nicht verfuegbar (#%u)\n",
          static_cast<unsigned>(g_mqtt_outbound_dropped));
      heap_caps_free(cmd);
      return;
    }

    mqtt_large_until = millis() + cmd->large_buffer_hold_ms;
    const bool ok = mqtt_client.publish(
        cmd->topic, cmd->payload, cmd->payload_len, cmd->retain);
    if (!ok) {
      Serial.printf("[MQTT] Worker: publish '%s' fehlgeschlagen\n", cmd->topic);
    }
    heap_caps_free(cmd);

    if ((++drained & 0x07) == 0) vTaskDelay(1);
  }
}

// Grow/Shrink-Logik des Empfangspuffers, 1:1 aus dem frueheren update()-Code:
// laeuft nur noch auf dem Worker, weil setBufferSize() den Client anfasst.
void HomeTilesNetworkManager::serviceBufferHousekeeping(uint32_t now_ms) {
  const uint32_t large_until = mqtt_large_until;
  if (large_until == 0) {
    // Kein Large-Fenster aktiv: Normalgroesse an die Media-Konfiguration
    // angleichen (Media-Tile hinzugefuegt/entfernt -> 24 KB rauf/runter).
    // OTA-Modus (1-KB-Puffer) nicht anfassen; Grows warten wie der
    // Large-Grow das Startup-Sturmfenster ab, Shrinks sind sofort okay.
    const uint16_t normal_size = mqttNormalBufferSize();
    if (mqtt_buffer_size != 0 && mqtt_buffer_size != kMqttBufferOta &&
        mqtt_buffer_size != normal_size &&
        (mqtt_buffer_size > normal_size ||
         mqtt_connected_at == 0 ||
         (uint32_t)(now_ms - mqtt_connected_at) >= kMqttStormWindowMs)) {
      setMqttBufferSize(normal_size, "media-config");
    }
    return;
  }
  if ((int32_t)(now_ms - large_until) >= 0) {
    mqtt_large_until = 0;
    setMqttBufferSize(mqttNormalBufferSize(), "normal");
  } else if (mqtt_buffer_size < kMqttBufferLarge &&
             (mqtt_connected_at == 0 ||
              (uint32_t)(now_ms - mqtt_connected_at) >= kMqttStormWindowMs)) {
    // Ausserhalb des Startup-Sturms sofort vergroessern; im Sturm bleibt der
    // Grow aufgeschoben und wird hier automatisch nachgeholt, sobald das
    // Fenster vorbei ist (frueher "large-deferred" in update()).
    setMqttBufferSize(kMqttBufferLarge, "large");
  }
}

// ========== Single-Owner MQTT: API fuer andere Tasks ==========
bool HomeTilesNetworkManager::mqttEnqueuePublish(const char* topic, const char* payload, bool retain) {
  const size_t len = payload ? strlen(payload) : 0;
  return enqueueOutboundCmd(MqttCmdKind::PUBLISH, topic,
                            reinterpret_cast<const uint8_t*>(payload), len, retain);
}

bool HomeTilesNetworkManager::mqttEnqueuePublish(const char* topic, const uint8_t* payload,
                                            size_t length, bool retain) {
  return enqueueOutboundCmd(MqttCmdKind::PUBLISH, topic, payload, length, retain);
}

bool HomeTilesNetworkManager::mqttEnqueuePublishPriority(const char* topic,
                                                    const char* payload,
                                                    bool retain) {
  const size_t len = payload ? strlen(payload) : 0;
  return enqueueOutboundCmd(MqttCmdKind::PUBLISH, topic,
                            reinterpret_cast<const uint8_t*>(payload), len,
                            retain, true);
}

bool HomeTilesNetworkManager::mqttEnqueuePublishWithLargeBuffer(
    const char* topic,
    const char* payload,
    bool retain,
    uint32_t hold_ms,
    bool priority) {
  const size_t len = payload ? strlen(payload) : 0;
  if (hold_ms == 0) hold_ms = 15000;
  return enqueueOutboundCmd(MqttCmdKind::PUBLISH, topic,
                            reinterpret_cast<const uint8_t*>(payload), len,
                            retain, priority, hold_ms);
}

bool HomeTilesNetworkManager::mqttEnqueueSubscribe(const char* topic) {
  return enqueueOutboundCmd(MqttCmdKind::SUBSCRIBE, topic, nullptr, 0, false);
}

bool HomeTilesNetworkManager::mqttEnqueueUnsubscribe(const char* topic) {
  return enqueueOutboundCmd(MqttCmdKind::UNSUBSCRIBE, topic, nullptr, 0, false);
}

bool HomeTilesNetworkManager::consumeMqttPostConnectPending() {
  if (!mqtt_post_connect_pending) return false;
  if (!mqtt_connected_flag) {
    mqtt_post_connect_pending = false;
    mqtt_post_connect_ready_at = 0;
    return false;
  }
  const uint32_t ready_at = mqtt_post_connect_ready_at;
  if (ready_at != 0 && (int32_t)(millis() - ready_at) < 0) {
    return false;
  }
  mqtt_post_connect_pending = false;  // einziger Konsument ist der Loop-Task
  mqtt_post_connect_ready_at = 0;
  return true;
}

void HomeTilesNetworkManager::disconnectMqtt() {
  if (!mqtt_enabled) return;
  mqtt_disconnect_requested = true;
  // Der Worker prueft das Flag am Anfang jeder Iteration (~2ms Takt); der
  // Aufrufer (Hotspot-Eintritt) ist selten und darf kurz warten.
  for (int i = 0; i < 100 && mqtt_disconnect_requested; ++i) {
    delay(5);
  }
  if (mqtt_disconnect_requested) {
    Serial.println("[MQTT] WARNUNG: Worker hat Disconnect-Request nicht bestaetigt");
  }
}

void HomeTilesNetworkManager::requestMqttReconfigure() {
  // Bewusst KEIN "if (!mqtt_enabled) return;" wie bei disconnectMqtt() --
  // der Worker soll mqtt_enabled hier gerade erst neu bestimmen (z.B. erste
  // MQTT-Konfiguration ueberhaupt, wo es bislang false war).
  mqtt_reconfig_requested = true;
  for (int i = 0; i < 100 && mqtt_reconfig_requested; ++i) {
    delay(5);
  }
  if (mqtt_reconfig_requested) {
    Serial.println("[MQTT] WARNUNG: Worker hat Reconfigure-Request nicht bestaetigt");
  }
}

void HomeTilesNetworkManager::prepareMqttForOta() {
  if (!mqtt_enabled) return;
  mqtt_ota_prep_requested = true;
  for (int i = 0; i < 100 && mqtt_ota_prep_requested; ++i) {
    delay(5);
  }
  if (mqtt_ota_prep_requested) {
    Serial.println("[OTA] WARNUNG: MQTT-Worker hat OTA-Vorbereitung nicht bestaetigt");
  }
}

void HomeTilesNetworkManager::deferMqttReconnect(uint32_t hold_ms) {
  if (!mqtt_enabled) return;
  if (hold_ms == 0) hold_ms = 6000;
  mqtt_reconnect_hold_until = millis() + hold_ms;
  mqtt_post_connect_pending = false;
  mqtt_post_connect_ready_at = 0;
  mqtt_large_until = 0;
  purgeOutboundQueue();
  Serial.printf("[MQTT] Reconnect fuer %u ms pausiert\n",
                static_cast<unsigned>(hold_ms));
}

// ========== MQTT-Status ==========
uint16_t HomeTilesNetworkManager::mqttNormalBufferSize() const {
  return mqtt_media_buffer_needed ? kMqttBufferMedia : kMqttBufferNormal;
}

bool HomeTilesNetworkManager::setMqttBufferSize(uint16_t size, const char* reason) {
  if (size == 0) return false;
  const uint16_t before = mqtt_client.getBufferSize();
  if (before == size) {
    mqtt_buffer_size = size;
    return true;
  }

  if (!mqtt_client.setBufferSize(size)) {
    Serial.printf("[MQTT] Buffer resize failed: %u -> %u bytes (%s)\n",
                  static_cast<unsigned>(before),
                  static_cast<unsigned>(size),
                  reason ? reason : "?");
    return false;
  }

  mqtt_buffer_size = mqtt_client.getBufferSize();
  Serial.printf("[MQTT] Buffer: %u -> %u bytes (%s, %s)\n",
                static_cast<unsigned>(before),
                static_cast<unsigned>(mqtt_buffer_size),
                reason ? reason : "?",
                mqtt_client.bufferInExternalRam() ? "PSRAM" : "internal");
  return true;
}

void HomeTilesNetworkManager::restoreMqttBufferNormal() {
  // Request-Flag statt direktem Client-Touch; der Worker setzt den Puffer
  // zurueck und hebt dabei auch eine evtl. OTA-Suspendierung wieder auf
  // (Aufrufer: restoreDisplayAfterOtaFailure()).
  mqtt_restore_normal_requested = true;
}

// ========== Network status ==========
bool HomeTilesNetworkManager::isNetworkConnected() const {
  return networkTransport.isConnected();
}

bool HomeTilesNetworkManager::isWifiConnected() const {
  return networkTransport.isWifiConnected();
}

// ========== Telemetrie senden ==========
void HomeTilesNetworkManager::publishTelemetry() {
  if (!isMqttConnected()) return;

  uint32_t now = millis();
  if (now - last_telemetry > 30000UL) {  // 30 Sekunden
    last_telemetry = now;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(now / 1000UL));
    const char* tele_topic = mqttTopics.topic(TopicKey::TELE_UP);
    if (tele_topic && *tele_topic) {
      mqttEnqueuePublish(tele_topic, buf, true);
    }
    mqttPublishHomeSnapshot();
  }
}

void HomeTilesNetworkManager::publishBridgeConfig() {
  if (!isMqttConnected()) return;
  if (!configManager.isConfigured()) return;

  const DeviceConfig& cfg = configManager.getConfig();
  char did[24];
  buildDeviceId(did, sizeof(did));
  String payload = haBridgeConfig.buildJsonPayload(did, cfg.mqtt_base_topic, cfg.ha_prefix);
  if (payload.isEmpty()) return;

  String topic = "tab5_lvgl/config/";
  topic += did;
  topic += "/bridge";
  const size_t packet_estimate = payload.length() + topic.length() + 16;
  if (packet_estimate > kMqttBufferLarge) {
    Serial.printf("[Network] Bridge config too large for MQTT buffer: %u > %u bytes\n",
                  static_cast<unsigned>(packet_estimate),
                  static_cast<unsigned>(kMqttBufferLarge));
  }
  mqttEnqueuePublishWithLargeBuffer(
      topic.c_str(), payload.c_str(), true, 15000);
  Serial.println("[Network] Home Assistant Bridge-Konfiguration publiziert");
}

const char* HomeTilesNetworkManager::getBridgeApplyTopic() const {
  return bridge_apply_topic_.length() ? bridge_apply_topic_.c_str() : nullptr;
}

void HomeTilesNetworkManager::publishBridgeRequest() {
  if (!isMqttConnected()) return;
  if (bridge_request_topic_.isEmpty()) return;
  mqttEnqueuePublishWithLargeBuffer(
      bridge_request_topic_.c_str(), "", false, 30000);
  Serial.println("[Network] Home Assistant Bridge-Aktualisierung angefordert");
}

const char* HomeTilesNetworkManager::getBridgeRequestTopic() const {
  return bridge_request_topic_.length() ? bridge_request_topic_.c_str() : nullptr;
}

const char* HomeTilesNetworkManager::getHistoryRequestTopic() const {
  return history_request_topic_.length() ? history_request_topic_.c_str() : nullptr;
}

const char* HomeTilesNetworkManager::getHistoryResponseTopic() const {
  return history_response_topic_.length() ? history_response_topic_.c_str() : nullptr;
}

const char* HomeTilesNetworkManager::getWeatherRequestTopic() const {
  return weather_request_topic_.length() ? weather_request_topic_.c_str() : nullptr;
}

const char* HomeTilesNetworkManager::getEnergyRequestTopic() const {
  return energy_request_topic_.length() ? energy_request_topic_.c_str() : nullptr;
}

const char* HomeTilesNetworkManager::getEnergyResponseTopic() const {
  return energy_response_topic_.length() ? energy_response_topic_.c_str() : nullptr;
}

const char* HomeTilesNetworkManager::getBridgeIconsTopic() const {
  return bridge_icons_topic_.length() ? bridge_icons_topic_.c_str() : nullptr;
}

// ========== mDNS-Advertising ==========
// Rein additiv: erlaubt der HA-Bridge (Zeroconf), das Geraet zu finden BEVOR
// es MQTT-Zugangsdaten hat. Laeuft ausschliesslich, solange noch keine
// MQTT-Konfiguration vorhanden ist (siehe update()). Ein kurzer Broker-Ausfall
// darf mDNS nicht staendig abbauen und neu anlegen: ESP-IDF gibt die dafuer
// reservierten internen/DMA-Bloecke auf P4 nicht in jedem Zyklus vollstaendig
// zurueck. Wird immer erst NACH
// webAdminServer.start() versucht -- ein haengender/fehlschlagender
// MDNS.begin() darf die admin-UI, die schon heute funktioniert, nicht
// verzoegern.
void HomeTilesNetworkManager::startMdns() {
  if (mdns_active) return;

  char did[24];
  buildDeviceId(did, sizeof(did));

  // device_id ist reiner Hex-Text (keine Unterstriche mehr) -- als
  // mDNS-Hostname direkt verwendbar, ohne RFC-952/1123-Zeichenersetzung.
  char hostname[24];
  snprintf(hostname, sizeof(hostname), "%s", did);

  logMdnsHeap("before-begin");
  if (!MDNS.begin(hostname)) {
    Serial.println("[mDNS] begin() fehlgeschlagen -- Advertising uebersprungen");
    return;
  }
  MDNS.addService("hometiles", "tcp", 80);

  // addServiceTxt() ist auf char*/const char*/String ueberladen. Ein Mix aus
  // String-Literalen (ueber die deprecated Literal->char*-Konvertierung auch
  // fuer die char*-Ueberladung gueltig) und einem char[]-Puffer macht die
  // Ueberladung mehrdeutig -> alle vier Argumente als benannte const char*-
  // Variablen uebergeben, dann ist nur noch die const-char*-Variante gueltig.
  const char* svc_name = "hometiles";
  const char* svc_proto = "tcp";
  const char* key_txtvers = "txtvers";
  const char* val_txtvers = "1";
  const char* key_device_id = "device_id";
  const char* val_device_id = did;
  const char* key_name = "name";
  const char* val_name = Device::displayName();
  const char* key_model = "model";
  const char* val_model = Device::profile().key;
  const DeviceConfig& cfg = configManager.getConfig();
  const char* key_base_topic = "base_topic";
  const char* val_base_topic = cfg.mqtt_base_topic;
  const char* key_ha_prefix = "ha_prefix";
  const char* val_ha_prefix = cfg.ha_prefix;

  MDNS.addServiceTxt(svc_name, svc_proto, key_txtvers, val_txtvers);
  MDNS.addServiceTxt(svc_name, svc_proto, key_device_id, val_device_id);
  MDNS.addServiceTxt(svc_name, svc_proto, key_name, val_name);
  MDNS.addServiceTxt(svc_name, svc_proto, key_model, val_model);
  MDNS.addServiceTxt(svc_name, svc_proto, key_base_topic, val_base_topic);
  MDNS.addServiceTxt(svc_name, svc_proto, key_ha_prefix, val_ha_prefix);
  mdns_active = true;
  logMdnsHeap("after-begin");
}

void HomeTilesNetworkManager::stopMdns() {
  if (!mdns_active) return;
  MDNS.end();
  mdns_active = false;
}

// ========== Update-Schleife (Loop-Task) ==========
void HomeTilesNetworkManager::update() {
  if (!configManager.isConfigured()) {
    return;
  }

  if (recoverWifiFromDmaStarvation()) {
    networkTransport.update();
    return;
  }

  networkTransport.update();
  uint32_t now_ms = millis();
  const bool wired_connected = isWiredConnected();
  const bool wired_link_up = isWiredLinkUp();

  // Der ESP-Hosted-Status bleibt bei einem abgestuerzten C6 gelegentlich auf
  // WL_CONNECTED stehen. MQTT und WebAdmin sind dann bereits tot, aber der
  // normale WiFi-Reconnect-Zweig wird nie betreten. Nach 15 s MQTT-Ausfall
  // deshalb einen echten, billigen Mode-RPC als Liveness-Probe senden. Ein
  // Broker-Ausfall laesst den RPC sofort erfolgreich zurueckkehren und darf
  // das WLAN nicht neu starten; nur der 5s-RPC-Timeout gilt als C6-Wedge.
  const bool wifi_claims_connected = networkTransport.isWifiConnected();
  if (mqtt_enabled && !mqtt_suspended && !wifi_manual_disconnect &&
      wifi_claims_connected && !isMqttConnected() && !wifi_wedge_latched) {
    if (wifi_mqtt_offline_since == 0) {
      wifi_mqtt_offline_since = now_ms;
      wifi_health_probe_at = now_ms + kWifiHealthProbeDelayMs;
    } else if ((int32_t)(now_ms - wifi_health_probe_at) >= 0) {
      wifi_health_probe_at = now_ms + kWifiHealthProbeIntervalMs;
      if (!probeWifiDriverHealth("MQTT offline bei gecachtem WiFi-Link")) {
        return;
      }
    }
  } else {
    wifi_mqtt_offline_since = 0;
    wifi_health_probe_at = 0;
  }

  if (wired_link_up && !wired_link_was_up) {
    wired_ip_wait_until = now_ms + kWiredDhcpWaitMs;
    wired_link_up_since = now_ms;
    Serial.printf("[Network] Ethernet-Link steht; warte bis zu %u ms auf DHCP\n",
                  static_cast<unsigned>(kWiredDhcpWaitMs));
  } else if (!wired_link_up) {
    wired_ip_wait_until = 0;
    wired_link_up_since = 0;
  }
  wired_link_was_up = wired_link_up;

  // WLAN-Treiber ist als tot markiert: Mit Ethernet-Link laeuft das Geraet
  // normal weiter. Faellt auch der Link weg, gibt es keinen Weg mehr zurueck
  // ins Netz - der sichere Neustart setzt den C6 mit zurueck (Bericht steht
  // bereits in /crashlog.txt).
  if (wifi_wedge_latched && !wired_link_up &&
      now_ms >= kWedgeRestartMinUptimeMs) {
    Serial.println(
        "[Network] WLAN-Wedge ohne Ethernet-Link: sicherer Neustart");
    Serial.flush();
    BoardHAL::prepareForRestart();
    delay(500);
    BoardHAL::restart();
    return;
  }

  const bool wired_dhcp_pending =
      wired_link_up && !wired_connected && wired_ip_wait_until != 0 &&
      static_cast<int32_t>(now_ms - wired_ip_wait_until) < 0;
  if (wired_dhcp_pending && networkTransport.isWifiDriverActive()) {
    // Close WiFi-owned sockets before the ESP-hosted driver is torn down.
    // The USB backend delays Ethernet DHCP until this memory is available.
    if (isMqttConnected()) disconnectMqtt();
    if (webAdminServer.isRunning()) webAdminServer.stop();
    stopMdns();
    stopWifiForWired();
    networkTransport.update();
  }

  if (wired_connected) {
    wired_ip_wait_until = 0;
    stopWifiForWired();
  } else if (wired_was_connected) {
    // Fester Ethernet-Modus: kein WLAN-Fallback mehr. Bis Kabel/DHCP
    // wiederkommen, ist das Geraet bewusst offline.
    Serial.println(
        "[Network] Ethernet getrennt; warte auf neuen Link (kein WLAN-Fallback)");
  }
  wired_was_connected = wired_connected;

  const uint32_t current_generation = networkTransport.generation();
  const bool transport_changed =
      current_generation != transport_generation_seen;
  if (transport_changed) {
    Serial.printf("[Network] Aktiver Transport: %s (generation=%u)\n",
                  networkTransport.activeName(),
                  static_cast<unsigned>(current_generation));
    transport_generation_seen = current_generation;

    // Recreate sockets after a default-route switch. Existing sockets can
    // otherwise stay bound to the interface that just disappeared.
    if (mqtt_enabled) {
      mqtt_reconfig_requested = true;
    }
    if (webAdminServer.isRunning()) {
      webAdminServer.stop();
    }
    stopMdns();
    // Treat a live route switch like a fresh connection edge so services
    // stopped above are immediately rebound to the new default interface.
    was_connected = false;
  }

  bool is_connected = networkTransport.isConnected();

  // Shared connectivity plus WiFi fallback management.
  if (!is_connected) {
    wifi_ps_state_known = false;

    // Nicht verbunden - Retry (ausser der Nutzer hat manuell getrennt).
    // connectWifi() selbst blockt im festen Ethernet-Modus.
    if (!wifi_manual_disconnect && !wired_dhcp_pending &&
        (int32_t)(now_ms - wifi_retry_at) >= 0) {
      connectWifi();
    }

    // WebAdmin stoppen wenn Verbindung verloren
    if (was_connected && webAdminServer.isRunning()) {
      webAdminServer.stop();
    }
    if (was_connected) {
      stopMdns();
    }
  } else {
    // Verbunden

    // Eine stehende WLAN-Verbindung beweist, dass der hosted-Treiber lebt -
    // langsame Einzelversuche davor waren dann Last, kein Wedge.
    if (networkTransport.isWifiConnected()) wifi_start_failures = 0;

    if (!was_connected) {
      logNetworkHeap(networkTransport.activeName());
    }

    // WebAdmin starten wenn gerade verbunden
    if (!was_connected && !webAdminServer.isRunning()) {
      webAdminServer.start();
    }

    // mDNS dient nur dem erstmaligen Pairing. Sobald MQTT konfiguriert ist,
    // bleibt es auch bei Broker-Reconnects aus. Das verhindert einen
    // begin/end-Zyklus samt DMA-Fragmentierung bei jeder kurzen Unterbrechung.
    if (configManager.hasMqttConfig()) {
      stopMdns();
    } else if (webAdminServer.isRunning()) {
      startMdns();
    }

    // NTP-Sync triggern bei neuer Verbindung
    if (!was_connected) {
      uiManager.scheduleNtpSync(0);
    }

    // MQTT-Verbindung/Socket/Puffer verwaltet komplett der Worker-Task
    // (serviceMqttWorker). Hier bleibt nur die Telemetrie, weil
    // publishTelemetry() -> mqttPublishHomeSnapshot() den Batterie-SoC per
    // I2C liest und deshalb auf dem Loop-Task bleiben muss; gesendet wird
    // ueber die Outbound-Queue.
    if (mqtt_enabled && isMqttConnected()) {
      publishTelemetry();
    }
  }

  // WiFi-Status für nächste Runde merken
  was_connected = is_connected;
}

// ========== WiFi Power Management ==========
void HomeTilesNetworkManager::setWifiPowerSaving(bool enable) {
  if (!isWifiConnected()) {
    wifi_ps_state_known = false;
    return;
  }

#if defined(CONFIG_IDF_TARGET_ESP32P4)
  // ESP32-P4 uses an SDIO/esp-hosted WiFi transport. Modem sleep can trigger
  // transport TX asserts under WebUI, MQTT, or media-cover traffic.
  enable = false;
#endif

  if (wifi_ps_state_known && wifi_ps_enabled == enable) {
    return;
  }

  if (enable) {
    if (wifi_sleep_profile) {
      // Sleep-Profil: Verbindung minimal halten, maximale Ersparnis.
      WiFi.setSleep(WIFI_PS_MAX_MODEM);
      WiFi.setTxPower(WIFI_POWER_5dBm);
      Serial.println("🔋 WiFi Sleep Profile: Max Modem Sleep + 5dBm");
    } else {
      // Normaler Stromsparmodus im Idle.
      WiFi.setSleep(WIFI_PS_MIN_MODEM);
      WiFi.setTxPower(WIFI_POWER_11dBm);
      Serial.println("🔋 WiFi Power Saving: Light Sleep + 11dBm");
    }
  } else {
    // Netzteilmodus: Volle Performance
    WiFi.setSleep(WIFI_PS_NONE);       // Kein Sleep
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximale Reichweite
    Serial.println("🔌 WiFi Full Power: No Sleep + 19.5dBm");
  }

  wifi_ps_state_known = true;
  wifi_ps_enabled = enable;
}

void HomeTilesNetworkManager::setSleepWifiProfile(bool enable) {
  if (wifi_sleep_profile == enable) return;
  wifi_sleep_profile = enable;
  // Profilwechsel soll beim naechsten setWifiPowerSaving() sicher angewendet werden.
  wifi_ps_state_known = false;
}
