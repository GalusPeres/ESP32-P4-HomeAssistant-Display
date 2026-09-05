#ifndef CRASH_LOG_H
#define CRASH_LOG_H

#include <Arduino.h>

// Makes crashes visible. On every panic (exception, task or interrupt
// watchdog, abort) the IDF panic handler already writes an ELF core dump into
// the coredump partition (see partitions.csv); it was simply never read back.
// This module reads the reset reason at boot and appends a readable summary to
// /crashlog.txt in LittleFS after an abnormal reset. Web Admin shows a "crash
// report" section that downloads the log (/api/crashlog) and the raw dump
// (/api/coredump).
//
// The raw dump resolves to a full stack trace on a PC. The build's ELF file is
// required and sits in the Arduino build folder after compiling:
//   pip install esp-coredump
//   esp-coredump info_corefile -t raw -c coredump.bin HomeTiles.ino.elf
namespace CrashLog {

// Path of the text log in LittleFS, also used by the Web Admin download.
constexpr const char* kLogPath = "/crashlog.txt";

// Call at boot, directly after the LittleFS mount. Writes an entry only when
// the reset was a crash or when flash holds a core dump that has not been
// logged yet, for example from a crash before this firmware revision.
void logBootDiagnostics();

// True when the coredump partition holds a valid core dump.
bool hasCoreDump();

// One-liner for the Web UI, for example "task=loopTask pc=0x4ff12345 ...".
// Empty when no dump is present.
String coreDumpSummaryLine();

// Appends a report to /crashlog.txt after a failed GitHub OTA install. The
// safe restart that follows leaves no core dump, so this entry replaces it as
// the diagnostic source: the error text plus the range and memory details from
// GithubUpdate::lastInstallDiag().
void appendOtaFailureReport(const char* target_tag, const String& error,
                            const String& detail);

// Appends a report when the ESP-Hosted WLAN driver on the C6 coprocessor stops
// responding (a "wedge": RPC timeouts, STA start failing repeatedly). As with
// the OTA report, that state ends in a controlled continuation or a safe
// restart without a core dump, so this entry is the only diagnostic trace.
void appendNetworkWedgeReport(const String& detail);

// Records a fail-closed restart after an accepted PPA operation or DSI
// framebuffer switch did not confirm completion. The normal software reset
// has no core dump, so this report is the persistent diagnostic evidence.
void appendDisplayPipelineTimeoutReport(const String& detail);

}  // namespace CrashLog

#endif  // CRASH_LOG_H
