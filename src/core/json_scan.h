#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Minimal, allocation-free scanners for the flat Home Assistant state payloads.
// The renderers and popups only need a handful of top-level fields, so they scan
// the payload in place instead of building a document. This header holds the
// shared primitives; every caller wraps them for its own string type.
//
// The scanners return offsets into the caller's buffer and never copy, so they
// can run on a payload that is only borrowed for the duration of one call.
// `json` must be NUL-terminated; `length` only bounds the scan.
namespace hometiles_json {

// A quote is a string delimiter only when the immediately preceding run of
// backslashes has even length. An odd run escapes the quote, while an even run
// ends with an escaped backslash and leaves the quote unescaped.
inline bool isUnescapedQuote(const char* json, int index) {
  if (!json || index < 0 || json[index] != '"') return false;
  int backslash_count = 0;
  for (int i = index - 1; i >= 0 && json[i] == '\\'; --i) {
    ++backslash_count;
  }
  return (backslash_count & 1) == 0;
}

// Offset of the first character after `"key":` and the blanks behind the colon,
// or -1 when the key is absent. The search is a plain substring match, which is
// what the historical HomeTiles scanners did: the payloads are flat, so a key
// name cannot collide with a nested object member.
inline int valueOffset(const char* json, int length, const char* key) {
  if (!json || length <= 0 || !key || !*key) return -1;
  const size_t key_length = strlen(key);
  // Quoted key plus the two quotes.
  const size_t pattern_length = key_length + 2;
  if (static_cast<size_t>(length) < pattern_length) return -1;
  const size_t limit = static_cast<size_t>(length) - pattern_length;
  int found = -1;
  for (size_t i = 0; i <= limit; ++i) {
    if (json[i] != '"') continue;
    if (memcmp(json + i + 1, key, key_length) != 0) continue;
    if (json[i + 1 + key_length] != '"') continue;
    found = static_cast<int>(i);
    break;
  }
  if (found < 0) return -1;
  int colon = -1;
  for (int i = found; i < length; ++i) {
    if (json[i] == ':') {
      colon = i;
      break;
    }
  }
  if (colon < 0) return -1;
  int pos = colon + 1;
  while (pos < length && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  return pos < length ? pos : -1;
}

// Span of a quoted string value, excluding the quotes. Escaped quotes stay
// inside the value instead of ending it.
inline bool stringSpan(const char* json, int length, const char* key,
                       int* begin, int* end) {
  const int pos = valueOffset(json, length, key);
  if (pos < 0 || json[pos] != '"') return false;
  const int start = pos + 1;
  bool escaped = false;
  for (int i = start; i < length; ++i) {
    const char c = json[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      if (begin) *begin = start;
      if (end) *end = i;
      return true;
    }
  }
  return false;
}

// Span of an object value, including the surrounding braces. Braces inside
// strings do not change the nesting depth.
inline bool objectSpan(const char* json, int length, const char* key,
                       int* begin, int* end) {
  const int pos = valueOffset(json, length, key);
  if (pos < 0 || json[pos] != '{') return false;
  int depth = 0;
  bool in_string = false;
  for (int i = pos; i < length; ++i) {
    const char c = json[i];
    if (isUnescapedQuote(json, i)) in_string = !in_string;
    if (in_string) continue;
    if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) {
        if (begin) *begin = pos;
        if (end) *end = i + 1;
        return true;
      }
    }
  }
  return false;
}

// Offset of the opening bracket of an array value, or -1 when `key` does not
// hold an array.
inline int arrayStart(const char* json, int length, const char* key) {
  const int pos = valueOffset(json, length, key);
  return (pos >= 0 && json[pos] == '[') ? pos : -1;
}

// Parses a bare number. A quoted number is rejected here; callers that accept
// both formats fall back to their string scanner.
inline bool number(const char* json, int length, const char* key, float* out) {
  const int pos = valueOffset(json, length, key);
  if (pos < 0) return false;
  const char* start = json + pos;
  char* parse_end = nullptr;
  const float value = strtof(start, &parse_end);
  if (!parse_end || parse_end == start) return false;
  if (out) *out = value;
  return true;
}

// Iterates the objects of an array value in place. `cursor` starts at the offset
// arrayStart() returned, or at 0 for the bracket-free inner text of an array,
// and is advanced past each object. Returns false at the closing bracket or at
// the end of the buffer. Braces inside strings are ignored.
inline bool nextObjectInArray(const char* json, int length, int* cursor,
                              int* begin, int* end) {
  if (!json || !cursor || *cursor < 0) return false;
  bool in_string = false;
  int depth = 0;
  int start = -1;
  for (int i = *cursor; i < length; ++i) {
    const char c = json[i];
    if (isUnescapedQuote(json, i)) in_string = !in_string;
    if (in_string) continue;
    if (c == ']' && depth == 0) {
      *cursor = i + 1;
      return false;
    }
    if (c == '{') {
      if (depth == 0) start = i;
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0 && start >= 0) {
        if (begin) *begin = start;
        if (end) *end = i + 1;
        *cursor = i + 1;
        return true;
      }
    }
  }
  *cursor = length;
  return false;
}

}  // namespace hometiles_json
