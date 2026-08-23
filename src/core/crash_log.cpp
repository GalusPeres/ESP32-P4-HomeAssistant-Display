#include "src/core/crash_log.h"

#include <LittleFS.h>
#include <esp_core_dump.h>
#include <esp_system.h>
#include <stdlib.h>

#include "src/core/firmware_version.h"
#include "src/devices/device.h"

namespace {

constexpr const char* kLogOldPath = "/crashlog.old.txt";
// Merkt sich den Fingerprint des zuletzt protokollierten Dumps, damit derselbe
// Dump bei normalen Reboots nicht bei jedem Boot erneut angehaengt wird.
constexpr const char* kStatePath = "/crashlog.state";
constexpr size_t kLogRotateBytes = 24 * 1024;

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external pin reset";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "panic (exception/abort)";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep wakeup";
    case ESP_RST_BROWNOUT: return "brownout (supply voltage dip)";
    case ESP_RST_SDIO: return "sdio reset";
    case ESP_RST_PWR_GLITCH: return "power glitch";
    case ESP_RST_CPU_LOCKUP: return "cpu lockup (double exception)";
    default: return "unknown";
  }
}

bool isCrashReset(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
    case ESP_RST_PWR_GLITCH:
    case ESP_RST_CPU_LOCKUP:
      return true;
    default:
      return false;
  }
}

// Kennung eines konkreten Dumps (PC/TCB/RA + Groesse reichen, um denselben
// Dump ueber Reboots hinweg wiederzuerkennen).
String dumpFingerprint(const esp_core_dump_summary_t& s) {
  size_t addr = 0;
  size_t size = 0;
  esp_core_dump_image_get(&addr, &size);
  char buf[48];
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  const unsigned long return_address =
      static_cast<unsigned long>(s.ex_info.exc_a[0]);
#else
  const unsigned long return_address =
      static_cast<unsigned long>(s.ex_info.ra);
#endif
  snprintf(buf, sizeof(buf), "%08lx-%08lx-%08lx-%08x",
           static_cast<unsigned long>(s.exc_pc),
           static_cast<unsigned long>(s.exc_tcb),
           return_address,
           static_cast<unsigned>(size));
  return String(buf);
}

String readStoredFingerprint() {
  File f = LittleFS.open(kStatePath, FILE_READ);
  if (!f) return String();
  String value = f.readStringUntil('\n');
  f.close();
  value.trim();
  return value;
}

void storeFingerprint(const String& fp) {
  File f = LittleFS.open(kStatePath, FILE_WRITE);
  if (!f) return;
  f.println(fp);
  f.close();
}

void rotateLogIfNeeded() {
  File f = LittleFS.open(CrashLog::kLogPath, FILE_READ);
  if (!f) return;
  const size_t size = f.size();
  f.close();
  if (size < kLogRotateBytes) return;
  LittleFS.remove(kLogOldPath);
  LittleFS.rename(CrashLog::kLogPath, kLogOldPath);
}

void appendSummary(File& f, const esp_core_dump_summary_t& s) {
  f.printf("Crashed task: %s\n", s.exc_task);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  f.printf("PC=0x%08lx A0=0x%08lx A1=0x%08lx\n",
           static_cast<unsigned long>(s.exc_pc),
           static_cast<unsigned long>(s.ex_info.exc_a[0]),
           static_cast<unsigned long>(s.ex_info.exc_a[1]));
  f.printf("EXCCAUSE=0x%08lx EXCVADDR=0x%08lx\n",
           static_cast<unsigned long>(s.ex_info.exc_cause),
           static_cast<unsigned long>(s.ex_info.exc_vaddr));
  f.printf("A0-A7: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
           static_cast<unsigned long>(s.ex_info.exc_a[0]),
           static_cast<unsigned long>(s.ex_info.exc_a[1]),
           static_cast<unsigned long>(s.ex_info.exc_a[2]),
           static_cast<unsigned long>(s.ex_info.exc_a[3]),
           static_cast<unsigned long>(s.ex_info.exc_a[4]),
           static_cast<unsigned long>(s.ex_info.exc_a[5]),
           static_cast<unsigned long>(s.ex_info.exc_a[6]),
           static_cast<unsigned long>(s.ex_info.exc_a[7]));
#else
  f.printf("PC=0x%08lx RA=0x%08lx SP=0x%08lx\n",
           static_cast<unsigned long>(s.exc_pc),
           static_cast<unsigned long>(s.ex_info.ra),
           static_cast<unsigned long>(s.ex_info.sp));
  f.printf("MCAUSE=0x%08lx MTVAL=0x%08lx MSTATUS=0x%08lx\n",
           static_cast<unsigned long>(s.ex_info.mcause),
           static_cast<unsigned long>(s.ex_info.mtval),
           static_cast<unsigned long>(s.ex_info.mstatus));
  f.printf("A0-A7: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
           static_cast<unsigned long>(s.ex_info.exc_a[0]),
           static_cast<unsigned long>(s.ex_info.exc_a[1]),
           static_cast<unsigned long>(s.ex_info.exc_a[2]),
           static_cast<unsigned long>(s.ex_info.exc_a[3]),
           static_cast<unsigned long>(s.ex_info.exc_a[4]),
           static_cast<unsigned long>(s.ex_info.exc_a[5]),
           static_cast<unsigned long>(s.ex_info.exc_a[6]),
           static_cast<unsigned long>(s.ex_info.exc_a[7]));
#endif
  f.printf("App ELF SHA256: %s\n",
           reinterpret_cast<const char*>(s.app_elf_sha256));
  f.print("Full core dump stored in flash - download via web admin "
          "(Crash Report section / GET /api/coredump)\n");
}

}  // namespace

namespace CrashLog {

bool hasCoreDump() {
  return esp_core_dump_image_check() == ESP_OK;
}

String coreDumpSummaryLine() {
  if (!hasCoreDump()) return String();
  auto* s = static_cast<esp_core_dump_summary_t*>(
      calloc(1, sizeof(esp_core_dump_summary_t)));
  if (!s) return String();
  String line;
  if (esp_core_dump_get_summary(s) == ESP_OK) {
    char buf[96];
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    snprintf(buf, sizeof(buf), "task=%s pc=0x%08lx exccause=0x%lx",
             s->exc_task,
             static_cast<unsigned long>(s->exc_pc),
             static_cast<unsigned long>(s->ex_info.exc_cause));
#else
    snprintf(buf, sizeof(buf), "task=%s pc=0x%08lx mcause=0x%lx",
             s->exc_task,
             static_cast<unsigned long>(s->exc_pc),
             static_cast<unsigned long>(s->ex_info.mcause));
#endif
    line = buf;
  }
  free(s);
  return line;
}

void logBootDiagnostics() {
  const esp_reset_reason_t reason = esp_reset_reason();
  const bool crash_reset = isCrashReset(reason);

  auto* summary = static_cast<esp_core_dump_summary_t*>(
      calloc(1, sizeof(esp_core_dump_summary_t)));
  const bool dump_valid =
      summary && hasCoreDump() && esp_core_dump_get_summary(summary) == ESP_OK;

  Serial.printf("[CrashLog] Reset-Grund: %s%s\n", resetReasonName(reason),
                dump_valid ? " | Core-Dump im Flash vorhanden" : "");

  // Normaler Boot ohne (neuen) Dump: nichts zu protokollieren. Ein bereits
  // bekannter Dump (Fingerprint gespeichert) wird nicht erneut angehaengt.
  bool log_dump = false;
  String fingerprint;
  if (dump_valid) {
    fingerprint = dumpFingerprint(*summary);
    log_dump = crash_reset || fingerprint != readStoredFingerprint();
  }
  if (!crash_reset && !log_dump) {
    free(summary);
    return;
  }

#if defined(DEVICE_ESP32_S3_RGB_480)
  Device::ScopedStorageWrite storage_write;
#endif
  rotateLogIfNeeded();
  File f = LittleFS.open(kLogPath, FILE_APPEND);
  if (!f) {
    Serial.println("[CrashLog] crashlog.txt nicht beschreibbar");
    free(summary);
    return;
  }

  f.printf("=== Boot after abnormal reset | firmware %s (%s) ===\n",
           FW_VERSION, crash_reset ? resetReasonName(reason) : "earlier crash");
  if (log_dump) {
    if (!crash_reset) {
      f.print("Found a stored core dump from an earlier crash:\n");
    }
    appendSummary(f, *summary);
    storeFingerprint(fingerprint);
  } else {
    f.print("No core dump stored (brownout/power loss does not write one)\n");
  }
  f.print("\n");
  f.close();
  free(summary);
  Serial.printf("[CrashLog] Absturz protokolliert -> %s\n", kLogPath);
}

void appendOtaFailureReport(const char* target_tag, const String& error,
                            const String& detail) {
#if defined(DEVICE_ESP32_S3_RGB_480)
  Device::ScopedStorageWrite storage_write;
#endif
  rotateLogIfNeeded();
  File f = LittleFS.open(kLogPath, FILE_APPEND);
  if (!f) {
    Serial.println("[CrashLog] crashlog.txt nicht beschreibbar");
    return;
  }
  f.printf("=== OTA install failed | firmware %s -> %s ===\n", FW_VERSION,
           (target_tag && *target_tag) ? target_tag : "?");
  f.printf("Error: %s\n", error.c_str());
  if (detail.length()) {
    f.print(detail);
    if (!detail.endsWith("\n")) f.print("\n");
  }
  f.print("Device performed a safe automatic restart (no crash, no core "
          "dump)\n\n");
  f.close();
  Serial.printf("[CrashLog] OTA-Fehlbericht protokolliert -> %s\n", kLogPath);
}

void appendNetworkWedgeReport(const String& detail) {
#if defined(DEVICE_ESP32_S3_RGB_480)
  Device::ScopedStorageWrite storage_write;
#endif
  rotateLogIfNeeded();
  File f = LittleFS.open(kLogPath, FILE_APPEND);
  if (!f) {
    Serial.println("[CrashLog] crashlog.txt nicht beschreibbar");
    return;
  }
  f.printf("=== WiFi driver wedge | firmware %s ===\n", FW_VERSION);
  if (detail.length()) {
    f.print(detail);
    if (!detail.endsWith("\n")) f.print("\n");
  }
  f.close();
  Serial.printf("[CrashLog] WLAN-Wedge protokolliert -> %s\n", kLogPath);
}

void appendDisplayPipelineTimeoutReport(const String& detail) {
#if defined(DEVICE_ESP32_S3_RGB_480)
  Device::ScopedStorageWrite storage_write;
#endif
  rotateLogIfNeeded();
  File f = LittleFS.open(kLogPath, FILE_APPEND);
  if (!f) {
    Serial.println("[CrashLog] Unable to write display pipeline timeout report");
    return;
  }
  f.printf("=== P4 display pipeline timeout | firmware %s ===\n", FW_VERSION);
  if (detail.length()) {
    f.print(detail);
    if (!detail.endsWith("\n")) f.print("\n");
  }
  f.print("Device performed a fail-closed software restart; no core dump is "
          "expected.\n\n");
  f.close();
  Serial.printf("[CrashLog] Display pipeline timeout report written -> %s\n",
                kLogPath);
}

}  // namespace CrashLog
