#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <Network.h>
// Vendored (not the global Arduino library): patched to insert a real
// vTaskDelay() periodically inside the packet-read loop, since
// readByte()/readPacket() otherwise only yield while WAITING for the next
// byte -- once a whole TCP segment is already buffered, they can walk
// through thousands of bytes with zero scheduling points. See
// src/network/vendor/pubsubclient/PubSubClient.cpp for details.
#include "src/network/vendor/pubsubclient/PubSubClient.h"

// Einzige Quelle fuer die device_id (volle 48-Bit-MAC als Hex-String, kein
// Praefix) -- von network_manager.cpp und mqtt_handlers.cpp genutzt, damit
// beide garantiert denselben Wert berechnen.
void buildDeviceId(char* buffer, size_t len);

// HomeTiles Network Manager - manages shared transports and MQTT.
//
// Single-Owner MQTT: das PubSubClient-Objekt (mqtt_client) wird nach init()
// ausschliesslich vom MQTT-Worker-Task angefasst (mqtt_worker_task in der
// .ino ruft serviceMqttWorker() in einer Schleife auf dem 2. Core). Alle
// anderen Tasks kommunizieren nur ueber die Outbound-Command-Queues
// (mqttEnqueue*) und volatile Request-Flags mit dem Worker -- dadurch
// braucht es keinerlei Mutex um den Client.
class HomeTilesNetworkManager {
public:
  // Initialisierung (laeuft in setup(), VOR dem Worker-Start). Baut u.a. die
  // Bridge-/Request-Topic-Strings EINMALIG -- frueher baute connectMqtt() sie
  // bei jedem Reconnect neu; sobald connectMqtt() auf dem Worker laeuft, waere
  // jedes String-Reassignment ein Race gegen die Getter-Reads des Loop-Tasks.
  void init();

  // Update-Schleife (Loop-Task): WiFi-Reconnect, WebAdmin, NTP, Telemetrie.
  // Die MQTT-Verbindung selbst verwaltet ausschliesslich der Worker.
  void update();

  // --- Single-Owner MQTT API ---
  void beginMqttWorker();    // einmalig aus setup(), VOR dem Task-Start (legt die Queue an)
  void serviceMqttWorker();  // Worker-Task-Body, eine Iteration pro Aufruf

  // Verbindungsstatus: volatile Flag, NUR vom Worker geschrieben -- jeder
  // andere Task darf es jederzeit lesen (ein Schreiber, viele Leser).
  bool isMqttConnected() const { return mqtt_connected_flag; }
  uint16_t getMqttBufferSize() const { return mqtt_buffer_size; }
  // Reclaimable ESP-Hosted DMA reserve currently held by the MQTT worker.
  // Camera/network guards count it as protected headroom without freeing it.
  size_t mqttDmaReserveBytes() const;

  // Von JEDEM Task sicher aufrufbar: kopiert Topic+Payload in einen (PSRAM-)
  // Block und reiht ihn nicht-blockierend in die passende Outbound-Queue ein.
  // Normale Bedienbefehle haben eine eigene Lane und werden nicht von
  // speicherintensiven History-/Energy-/Bridge-Anfragen blockiert.
  bool mqttEnqueuePublish(const char* topic, const char* payload, bool retain);
  bool mqttEnqueuePublish(const char* topic, const uint8_t* payload, size_t length, bool retain);
  // Interaktive kleine Requests duerfen vor einem langen Subscribe-Sturm
  // einsortiert werden. Der Worker bleibt weiterhin der einzige Client-Owner.
  bool mqttEnqueuePublishPriority(const char* topic, const char* payload,
                                  bool retain);
  // Publish, fuer dessen Versand bzw. erwartete Antwort der grosse MQTT-
  // Puffer benoetigt wird. Es landet in einer separaten Large-Lane, die bei
  // knapper DMA-Reserve warten darf, ohne normale Bedienbefehle anzuhalten.
  bool mqttEnqueuePublishWithLargeBuffer(const char* topic,
                                         const char* payload,
                                         bool retain,
                                         uint32_t hold_ms,
                                         bool priority = false);
  bool mqttEnqueueSubscribe(const char* topic);
  bool mqttEnqueueUnsubscribe(const char* topic);

  // Nach erfolgreichem (Re-)Connect setzt der Worker ein Pending-Flag; der
  // Loop-Task konsumiert es (mqttServicePostConnect in mqtt_handlers.cpp) und
  // faehrt die App-Ebene hoch: Subscribes/Discovery/DeviceSettings/Snapshot
  // fassen Flash, LVGL-Grids und I2C an und duerfen deshalb NICHT auf dem
  // Worker laufen -- ihre publishes/subscribes kommen per Queue zurueck.
  bool consumeMqttPostConnectPending();

  // Request-Flags an den Worker, mit kurzem bounded Warten (<=500ms) auf die
  // Bestaetigung -- die Aufrufer (Hotspot-Eintritt, OTA-Start) sind selten
  // und nicht zeitkritisch.
  void disconnectMqtt();
  void prepareMqttForOta();
  void deferMqttReconnect(uint32_t hold_ms = 6000);

  // Nach dem Speichern neuer MQTT-Einstellungen im Web-Admin: trennt eine
  // laufende Verbindung, liest mqtt_enabled/Host/Port frisch aus dem
  // ConfigManager und verbindet sofort neu -- ohne Geraete-Neustart. Anders
  // als disconnectMqtt() NICHT auf mqtt_enabled gegated, da genau dieses Flag
  // hier live neu gesetzt werden soll (Erstkonfiguration, Host geleert etc.).
  void requestMqttReconfigure();

  // Das eigentliche setBufferSize() macht ausschliesslich der Worker.
  void restoreMqttBufferNormal();

  // Media-Tiles konfiguriert? Dann faehrt der Worker den "normalen" Puffer auf
  // kMqttBufferMedia (24 KB) statt 16 KB, damit Bridge-States mit eingebettetem
  // Cover (~19 KB) nicht von PubSubClient verworfen werden. Wird beim Boot aus
  // der Tile-Config gesetzt und bei jedem Route-Rebuild aktuell gehalten.
  void setMqttMediaBufferNeeded(bool needed) { mqtt_media_buffer_needed = needed; }

  // Shared network status plus WiFi-specific status for the WiFi settings UI.
  bool isNetworkConnected() const;
  bool isWifiConnected() const;
  bool wasPreviouslyConnected() const { return was_connected; }

  // Verbindung herstellen
  void connectWifi();

  // ESP32-P4/ESP-Hosted-Liveness-Probe. WiFi.status() ist bei einem
  // festgefahrenen C6 nur ein gecachter Zustand und kann weiterhin
  // "verbunden" melden. getMode() ist dagegen ein echtes RPC: gesund kehrt
  // es sofort zurueck, beim Wedge nach dem 5s-Timeout. In diesem Fall wird
  // der bereits vorhandene sichere Recovery-Pfad ausgeloest.
  bool probeWifiDriverHealth(const char* context);

  // Vom Nutzer angefordertes Trennen (WLAN-Popup "Trennen"): trennt und
  // unterdrueckt jeden Auto-Reconnect, bis wieder manuell verbunden wird
  // (connectWifi) oder das Geraet neu startet. Zugangsdaten bleiben
  // gespeichert - nach einem Reboot verbindet das Geraet normal.
  void disconnectWifiManual();
  bool isWifiManuallyDisconnected() const { return wifi_manual_disconnect; }

  // Telemetrie (Loop-Task; sendet ueber die Outbound-Queue)
  void publishTelemetry();
  void publishBridgeConfig();
  // Leere Requests sind der leichte periodische Dirty-Check. Ein explizites
  // "force" wird nur fuer den manuellen Admin-Refresh gesendet.
  void publishBridgeRequest(bool force = false);
  const char* getBridgeApplyTopic() const;
  const char* getBridgeRequestTopic() const;
  const char* getHistoryRequestTopic() const;
  const char* getHistoryResponseTopic() const;
  const char* getWeatherRequestTopic() const;
  const char* getEnergyRequestTopic() const;
  const char* getEnergyResponseTopic() const;
  const char* getBridgeIconsTopic() const;

  // WiFi Power Management
  void setWifiPowerSaving(bool enable);
  void setSleepWifiProfile(bool enable);

  // mDNS-Advertising stoppen (z.B. beim Eintritt in den Hotspot/AP-Modus,
  // der STA-seitig ohnehin nicht laeuft). startMdns() bleibt intern -- wird
  // nur von update() auf der bestehenden Connect-Flanke ausgeloest.
  void stopMdns();

private:
  NetworkClient net_client;
  PubSubClient mqtt_client;  // nach init() NUR noch vom Worker-Task beruehrt

  uint32_t wifi_retry_at = 0;
  uint32_t wired_ip_wait_until = 0;
  bool wifi_manual_disconnect = false;  // Loop-Task: setzt/liest, UI liest
  bool wifi_suspended_for_wired = false;
  bool wired_link_was_up = false;
  bool wired_was_connected = false;
  // Loop-Task: millis() der letzten Link-up-Flanke (0 = Link unten). Solange
  // der Ethernet-Link steht, darf WiFi fruehestens nach kWiredLinkWifiBlockMs
  // ohne IP starten - sonst sabotiert der hosted-Start die DMA-Allokation des
  // Ethernet-Backends (Feldtest 2026-07-16).
  uint32_t wired_link_up_since = 0;
  // Loop-Task: STA-Start-Fehlversuche in Folge. Ab kWifiStartWedgeThreshold
  // gilt der ESP-Hosted-Treiber als tot (C6 antwortet nicht mehr auf RPCs).
  uint8_t wifi_start_failures = 0;
  // Loop-Task: WLAN-Treiber fuer tot erklaert. Mit Ethernet-Link laeuft das
  // Geraet ohne WiFi weiter; faellt auch Ethernet weg, hilft nur noch der
  // sichere Neustart (setzt den C6 mit zurueck).
  bool wifi_wedge_latched = false;
  // Solange MQTT offline ist, obwohl der WiFi-Transport weiterhin eine
  // Verbindung behauptet, nach einer Schonfrist einen echten Hosted-RPC
  // pruefen. So bleibt ein toter C6 nicht unbegrenzt im gecachten
  // WL_CONNECTED-Zustand haengen.
  uint32_t wifi_mqtt_offline_since = 0;
  uint32_t wifi_health_probe_at = 0;
  uint32_t mqtt_retry_at = 0;      // worker-only
  uint8_t mqtt_connect_failures = 0;  // worker-only: Fehlversuche in Folge
  uint32_t last_telemetry = 0;
  bool was_connected = false;
  uint32_t transport_generation_seen = 0;
  bool mqtt_enabled = false;
  bool mdns_active = false;
  // Build-compat: used by older/newer network_manager.cpp variants.
  bool wifi_ps_state_known = false;
  bool wifi_ps_enabled = false;
  bool wifi_sleep_profile = false;
  uint32_t mqtt_connected_at = 0;  // worker-only: millis() des letzten Connects

  // Cross-Task-Signale. Einfache aligned bool/uint-Reads/Writes sind auf
  // dieser Architektur atomar; jedes Flag hat genau einen Schreiber je
  // Richtung (Request: Fremd-Task setzt, Worker loescht -- Status: Worker
  // setzt, Fremd-Tasks lesen).
  volatile bool mqtt_connected_flag = false;
  volatile bool mqtt_post_connect_pending = false;
  volatile bool mqtt_disconnect_requested = false;
  volatile bool mqtt_reconfig_requested = false;
  volatile bool mqtt_ota_prep_requested = false;
  volatile bool mqtt_restore_normal_requested = false;
  volatile bool mqtt_suspended = false;  // OTA laeuft: Worker ruehrt nichts mehr an
  // Worker setzt nach anhaltender DMA-Starvation; der Loop-Task baut daraufhin
  // nur WLAN/SDIO kontrolliert neu auf. Bis dahin fasst der Worker den
  // Netzwerkclient nicht mehr an.
  volatile bool mqtt_transport_recovery_requested = false;
  // Requests, die den Engpass sichtbar gemacht haben, bleiben ueber den
  // Recovery-Reconnect erhalten und werden danach erneut abgearbeitet.
  volatile bool mqtt_preserve_outbound_on_connect = false;
  volatile uint32_t mqtt_reconnect_hold_until = 0;
  volatile uint32_t mqtt_post_connect_ready_at = 0;
  volatile uint32_t mqtt_large_until = 0;
  volatile uint16_t mqtt_buffer_size = 0;  // Spiegel der Client-Puffergroesse, Worker pflegt
  volatile bool mqtt_media_buffer_needed = false;  // Media-Tiles vorhanden -> 24-KB-Normalpuffer

  // Zielgroesse des "normalen" Puffers abhaengig von der Media-Konfiguration.
  uint16_t mqttNormalBufferSize() const;

  // Einmalig in init() gebaut (vor Worker-Start), danach nur noch gelesen.
  String bridge_apply_topic_;
  String bridge_request_topic_;
  String history_request_topic_;
  String history_response_topic_;
  String weather_request_topic_;
  String energy_request_topic_;
  String energy_response_topic_;
  String bridge_icons_topic_;

  // worker-only (nach init()):
  void connectMqtt();
  void drainOutboundQueues(uint8_t max_commands);
  void serviceBufferHousekeeping(uint32_t now_ms);
  bool setMqttBufferSize(uint16_t size, const char* reason);

  // Wired transports are exclusive with STA WiFi. This matters especially on
  // ESP32-P4, where an otherwise idle WiFi connection still keeps the hosted
  // SDIO RX path and its DMA allocations alive.
  bool isWiredConnected() const;
  bool isWiredLinkUp() const;
  bool isWifiStationEnabled() const;
  bool ensureWifiStationStarted();
  void stopWifiForWired();
  bool recoverWifiFromDmaStarvation();

  // ESP-Hosted-Wedge (C6 antwortet nicht mehr): Bericht nach /crashlog.txt,
  // dann mit Ethernet weiterlaufen oder - ohne Ethernet-Link - sicherer
  // Neustart, der den C6 mit zuruecksetzt. Laeuft auf dem Loop-Task.
  void handleWifiDriverWedge(const char* context = nullptr);

  // mDNS-Start (Loop-Task, gleiche connect-Flanke wie webAdminServer). Rein
  // additiv fuers Zeroconf-Discovery der HA-Bridge -- beeinflusst weder MQTT
  // noch WebAdmin, wird bei Fehlschlag stillschweigend uebersprungen statt
  // irgendetwas anderes zu blockieren.
  void startMdns();
};

// Globale Instanz
extern HomeTilesNetworkManager networkManager;

#endif // NETWORK_MANAGER_H
