#ifndef HA_BRIDGE_CONFIG_H
#define HA_BRIDGE_CONFIG_H

#include <Arduino.h>
#include <strings.h>
#include <map>
#include <string>
#include <utility>
#include <esp_heap_caps.h>

// Allocator, der alles in den PSRAM legt (MALLOC_CAP_SPIRAM). Der Standard-
// malloc routet kleine Allokationen IMMER in den internen Heap -- std::map-
// Knoten und String-Puffer des Entity-Index wuerden sonst die knappen ~236KB
// internes SRAM belegen, die fuer UI-Renderband und WiFi reserviert sind.
template <typename T>
struct PsramAllocator {
  using value_type = T;
  PsramAllocator() noexcept = default;
  template <typename U>
  PsramAllocator(const PsramAllocator<U>&) noexcept {}
  T* allocate(size_t n) {
    void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // Notnagel interner Heap: ein Allocator darf nie nullptr liefern, der
    // Container wuerde sonst an Adresse 0 schreiben.
    if (!p) p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_8BIT);
    if (!p) abort();
    return static_cast<T*>(p);
  }
  void deallocate(T* p, size_t) noexcept { heap_caps_free(p); }
  template <typename U>
  bool operator==(const PsramAllocator<U>&) const noexcept { return true; }
  template <typename U>
  bool operator!=(const PsramAllocator<U>&) const noexcept { return false; }
};

// std::string mit PSRAM-Allocator: kurze Werte (<=15 Zeichen, SSO) leben
// direkt im Map-Knoten (der selbst im PSRAM liegt), laengere Puffer holt der
// Allocator ebenfalls aus dem PSRAM. Arduino String kann das nicht -- seine
// Puffer kommen immer aus dem internen Heap.
using PsString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

// Case-insensitive geordnete Map fuer Entity-Keys -- die Text-Blob-Maps
// unten matchen Keys ueberall mit strncasecmp/equalsIgnoreCase, der Index
// muss sich identisch verhalten. is_transparent erlaubt find(const char*)
// ohne temporaere Key-Kopie.
struct HaEntityKeyLess {
  using is_transparent = void;
  bool operator()(const PsString& a, const PsString& b) const {
    return strcasecmp(a.c_str(), b.c_str()) < 0;
  }
  bool operator()(const PsString& a, const char* b) const {
    return strcasecmp(a.c_str(), b) < 0;
  }
  bool operator()(const char* a, const PsString& b) const {
    return strcasecmp(a, b.c_str()) < 0;
  }
};
using HaEntityKeyMap =
    std::map<PsString, PsString, HaEntityKeyLess,
             PsramAllocator<std::pair<const PsString, PsString>>>;

static constexpr size_t HA_SENSOR_SLOT_COUNT = 6;
static constexpr size_t HA_SCENE_SLOT_COUNT = 6;

struct HaBridgeConfigData {
  String sensors_text;
  String energy_text;
  String weathers_text;
  String lights_text;
  String switches_text;
  String media_players_text;
  String climates_text;
  String covers_text;
  String cameras_text;
  String scene_alias_text;
  String sensor_slots[HA_SENSOR_SLOT_COUNT];
  String scene_slots[HA_SCENE_SLOT_COUNT];
  String sensor_units_map;
  String sensor_names_map;
  String sensor_values_map;
  String entity_icons_map;
  String sensor_titles[HA_SENSOR_SLOT_COUNT];
  String sensor_custom_units[HA_SENSOR_SLOT_COUNT];
  String scene_titles[HA_SCENE_SLOT_COUNT];
  uint32_t sensor_colors[HA_SENSOR_SLOT_COUNT];  // RGB Hex (0 = Standard 0x2A2A2A)
  uint32_t scene_colors[HA_SCENE_SLOT_COUNT];    // RGB Hex (0 = Standard 0x353535)
};

class HaBridgeConfig {
public:
  HaBridgeConfig();

  bool load();
  bool save(const HaBridgeConfigData& data);
  bool applyJson(const char* json_payload, bool* out_reload = nullptr, bool* out_icons_changed = nullptr);

  const HaBridgeConfigData& get() const { return data; }
  bool hasData() const;
  String findSensorUnit(const String& entity_id) const;
  String findSensorName(const String& entity_id) const;
  String findSensorInitialValue(const String& entity_id) const;
  String findEntityIcon(const String& entity_id) const;
  // const char*-Varianten fuer Aufrufer, die selbst keine Arduino Strings
  // halten (z.B. der PSRAM-Ordner-Entity-Cache) -- dank is_transparent
  // komplett allokationsfrei bis auf den Rueckgabewert.
  String findSensorUnit(const char* entity_id) const;
  String findSensorName(const char* entity_id) const;
  String findSensorInitialValue(const char* entity_id) const;
  String findEntityIcon(const char* entity_id) const;
  String findSceneEntity(const String& alias) const;

  // Update live sensor value (for web interface)
  void updateSensorValue(const String& entity_id, const String& value);
  void registerSensorMeta(const String& entity_id, const String& name, const String& unit);
  void updateEntityMeta(const String& entity_id, const String& name, const String& unit, const String& icon);
  bool applyIconUpdate(const char* json_payload);

  String buildJsonPayload(const char* device_id,
                          const char* base_topic,
                          const char* ha_prefix) const;

private:
  HaBridgeConfigData data;

  // Lookup-Index ueber den 4 "key=value\n"-Text-Blobs (sensor_units_map etc.).
  // Die Blobs bleiben das fuehrende Format (Web-Admin liest sie direkt,
  // applyJson tauscht sie als Ganzes) -- aber ALLE find*-Lookups laufen ueber
  // diese Maps statt den Blob linear zu durchsuchen. Ein einziger Lookup auf
  // einem ueber die Laufzeit gewachsenen Blob war fuer sich allein schon ein
  // mehrere-ms-Block; der Bridge-Cache-Refresh macht ~150 davon am Stueck
  // (gemessen: bridge_cache=2324ms im [LoopGap]-Log). Bewusst NICHT in
  // HaBridgeConfigData: applyJson kopiert das ganze struct (merged = data),
  // die Indexe sollen da nicht mitkopiert werden.
  HaEntityKeyMap units_index_;
  HaEntityKeyMap names_index_;
  HaEntityKeyMap values_index_;
  HaEntityKeyMap icons_index_;
  // Nach jedem Blob-Komplett-Austausch aufrufen (load/save/applyJson); die
  // Einzel-Updates (updateSensorValue etc.) pflegen Blob und Index parallel.
  void rebuildEntityIndexes();

  static void appendJsonEscaped(String& out, const String& value);
  static void appendSensorsJson(String& out, const String& text);
  static void appendSceneMapJson(String& out, const String& text);
  static String normalizeLine(const String& line);
};

extern HaBridgeConfig haBridgeConfig;

#endif // HA_BRIDGE_CONFIG_H
