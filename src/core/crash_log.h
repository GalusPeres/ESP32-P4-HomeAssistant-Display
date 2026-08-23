#ifndef CRASH_LOG_H
#define CRASH_LOG_H

#include <Arduino.h>

// Macht Abstuerze sichtbar: Der IDF-Panic-Handler schreibt bei jeder Panic
// (Exception, Task-/Interrupt-Watchdog, abort) automatisch einen Core-Dump im
// ELF-Format in die coredump-Partition (siehe partitions.csv) - er wurde nur
// bisher nie ausgelesen. Dieses Modul liest beim Boot den Reset-Grund aus und
// haengt nach einem abnormalen Reset eine lesbare Zusammenfassung an
// /crashlog.txt im LittleFS an. Web-Admin: Abschnitt "Absturzbericht" mit
// Download des Logs (/api/crashlog) und des rohen Dumps (/api/coredump).
//
// Der rohe Dump laesst sich am PC zu einem vollstaendigen Stacktrace
// aufloesen (ELF des Builds noetig, liegt nach dem Kompilieren im
// Arduino-Build-Ordner):
//   pip install esp-coredump
//   esp-coredump info_corefile -t raw -c coredump.bin HomeTiles.ino.elf
namespace CrashLog {

// Pfad des Text-Logs im LittleFS (auch vom Web-Admin-Download verwendet).
constexpr const char* kLogPath = "/crashlog.txt";

// Beim Boot direkt nach dem LittleFS-Mount aufrufen. Schreibt nur dann einen
// Eintrag, wenn der Reset ein Absturz war oder ein noch nicht protokollierter
// Core-Dump im Flash liegt (z.B. von einem Crash vor diesem Firmware-Stand).
void logBootDiagnostics();

// True, wenn ein gueltiger Core-Dump in der coredump-Partition liegt.
bool hasCoreDump();

// Einzeiler fuer die Web-UI, z.B. "task=loopTask pc=0x4ff12345 ...";
// leer, wenn kein Dump vorliegt.
String coreDumpSummaryLine();

// Haengt nach einem fehlgeschlagenen GitHub-OTA-Install einen Bericht an
// /crashlog.txt an. Der anschliessende sichere Neustart hinterlaesst keinen
// Core-Dump - dieser Eintrag ersetzt ihn als Diagnosequelle (Fehlertext plus
// Range-/Speicher-Details aus GithubUpdate::lastInstallDiag()).
void appendOtaFailureReport(const char* target_tag, const String& error,
                            const String& detail);

// Haengt einen Bericht an, wenn der ESP-Hosted-WLAN-Treiber (C6-Coprozessor)
// nicht mehr antwortet ("Wedge": RPC-Timeouts, STA-Start schlaegt wiederholt
// fehl). Wie beim OTA-Bericht: Der Zustand endet in einem kontrollierten
// Weiterlauf oder sicheren Neustart ohne Core-Dump - dieser Eintrag ist die
// einzige Diagnosespur.
void appendNetworkWedgeReport(const String& detail);

// Records a fail-closed restart after an accepted PPA operation or DSI
// framebuffer switch did not confirm completion. The normal software reset
// has no core dump, so this report is the persistent diagnostic evidence.
void appendDisplayPipelineTimeoutReport(const String& detail);

}  // namespace CrashLog

#endif  // CRASH_LOG_H
