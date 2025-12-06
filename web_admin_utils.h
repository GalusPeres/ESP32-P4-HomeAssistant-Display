#pragma once
#include <Arduino.h>
#include <vector>

// Structure for scene options (alias and entity)
struct SceneOption {
  String alias;
  String entity;
};

// Utility functions for web admin operations

// Safely copy a String to a char buffer with null termination
void copyToBuffer(char* dest, size_t max_len, const String& value);

// Append HTML-escaped version of value to out
void appendHtmlEscaped(String& out, const String& value);

// Convert identifier to human-readable format (e.g., "sensor.temp" -> "Sensor Temp")
String humanizeIdentifier(const String& raw, bool strip_domain);

// Parse newline-separated sensor list
std::vector<String> parseSensorList(const String& raw);

// Parse scene list with alias=entity format
std::vector<SceneOption> parseSceneList(const String& raw);

// Normalize sensor selection against available options
String normalizeSensorSelection(const String& selection,
                                const std::vector<String>& options);

// Normalize scene selection against available options
String normalizeSceneSelection(const String& selection,
                               const std::vector<SceneOption>& options);

// Lookup key-value pair in text (key=value format, one per line)
String lookupKeyValue(const String& text, const String& key);
