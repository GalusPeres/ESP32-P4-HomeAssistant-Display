// Host-compiled unit test for the shared Home Assistant payload scanners in
// src/core/json_scan.h. The renderers and popups used to carry their own copies
// of these scanners; this test pins the behavior the shared header replaces.
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const repoRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const tempRoot = mkdtempSync(join(tmpdir(), "hometiles-json-scan-"));
const sourcePath = join(tempRoot, "json_scan_test.cpp");
const outputPath = join(
  tempRoot,
  process.platform === "win32" ? "json_scan_test.exe" : "json_scan_test",
);

const source = String.raw`
#include "src/core/json_scan.h"

#include <assert.h>
#include <string.h>

using namespace hometiles_json;

static int len(const char* text) { return (int)strlen(text); }

static bool spanEquals(const char* json, int begin, int end,
                       const char* expected) {
  const int expected_length = len(expected);
  return end - begin == expected_length &&
         memcmp(json + begin, expected, (size_t)expected_length) == 0;
}

int main() {
  const char* state =
      "{\"state\":\"on\",\"brightness\": 128,\"friendly_name\":\"Lamp\","
      "\"attributes\":{\"hs_color\":[30,50],\"nested\":{\"a\":1}},"
      "\"forecast\":[{\"temp\":21.5}],\"unit\":\"\\u00b0C\"}";
  const int state_length = len(state);

  // Missing keys and defensive arguments.
  assert(valueOffset(state, state_length, "absent") == -1);
  assert(valueOffset(nullptr, 10, "state") == -1);
  assert(valueOffset(state, 0, "state") == -1);
  assert(valueOffset(state, state_length, "") == -1);
  assert(valueOffset(state, state_length, nullptr) == -1);

  // A key must match in full: "state" must not be found through "states".
  const char* plural = "{\"states\":\"on\"}";
  assert(valueOffset(plural, len(plural), "state") == -1);
  assert(valueOffset(plural, len(plural), "states") != -1);

  int begin = -1;
  int end = -1;
  assert(stringSpan(state, state_length, "state", &begin, &end));
  assert(spanEquals(state, begin, end, "on"));
  assert(stringSpan(state, state_length, "friendly_name", &begin, &end));
  assert(spanEquals(state, begin, end, "Lamp"));
  // A number is not a string value, and the scanner must not walk on to the
  // next quoted key instead.
  assert(!stringSpan(state, state_length, "brightness", &begin, &end));
  // An escaped quote stays inside the value.
  const char* quoted = "{\"media_title\":\"O\\\"Brien\",\"next\":\"x\"}";
  assert(stringSpan(quoted, len(quoted), "media_title", &begin, &end));
  assert(spanEquals(quoted, begin, end, "O\\\"Brien"));
  // An unterminated string is rejected rather than returning a partial span.
  const char* truncated = "{\"state\":\"on";
  assert(!stringSpan(truncated, len(truncated), "state", &begin, &end));

  assert(objectSpan(state, state_length, "attributes", &begin, &end));
  assert(spanEquals(state, begin, end,
                    "{\"hs_color\":[30,50],\"nested\":{\"a\":1}}"));
  assert(!objectSpan(state, state_length, "state", &begin, &end));
  // A brace inside a string must not change the nesting depth.
  const char* braced = "{\"attributes\":{\"name\":\"a}b\"},\"tail\":1}";
  assert(objectSpan(braced, len(braced), "attributes", &begin, &end));
  assert(spanEquals(braced, begin, end, "{\"name\":\"a}b\"}"));

  assert(arrayStart(state, state_length, "forecast") >= 0);
  assert(state[arrayStart(state, state_length, "forecast")] == '[');
  assert(arrayStart(state, state_length, "attributes") == -1);
  assert(arrayStart(state, state_length, "absent") == -1);

  float value = -1.0f;
  assert(number(state, state_length, "brightness", &value));
  assert(value == 128.0f);
  const char* negative = "{\"temperature\": -3.5,\"unit\":\"C\"}";
  assert(number(negative, len(negative), "temperature", &value));
  assert(value == -3.5f);
  // Quoted numbers and non-numeric values are rejected; callers fall back to
  // their string scanner for those.
  const char* text_number = "{\"temperature\":\"21.5\"}";
  assert(!number(text_number, len(text_number), "temperature", &value));
  const char* null_value = "{\"temperature\":null}";
  assert(!number(null_value, len(null_value), "temperature", &value));
  assert(!number(state, state_length, "absent", &value));

  // Tabs behind the colon are skipped like spaces.
  const char* tabbed = "{\"brightness\":\t64}";
  assert(number(tabbed, len(tabbed), "brightness", &value));
  assert(value == 64.0f);

  // Array iteration, starting at the bracket the payload reports.
  int cursor = arrayStart(state, state_length, "forecast");
  assert(cursor >= 0);
  assert(nextObjectInArray(state, state_length, &cursor, &begin, &end));
  assert(spanEquals(state, begin, end, "{\"temp\":21.5}"));
  assert(!nextObjectInArray(state, state_length, &cursor, &begin, &end));

  // The same iterator walks the bracket-free inner text of an array, skips
  // nested objects and braces inside strings, and stops at the closing bracket.
  const char* items =
      "[{\"a\":1,\"in\":{\"b\":2}},{\"c\":\"}{\"},{\"d\":4}]\"trailing\":9";
  const int items_length = len(items);
  cursor = 0;
  const char* expected[] = {
      "{\"a\":1,\"in\":{\"b\":2}}", "{\"c\":\"}{\"}", "{\"d\":4}"};
  for (int index = 0; index < 3; ++index) {
    assert(nextObjectInArray(items, items_length, &cursor, &begin, &end));
    assert(spanEquals(items, begin, end, expected[index]));
  }
  assert(!nextObjectInArray(items, items_length, &cursor, &begin, &end));
  // Iteration stopped at the bracket instead of running into the trailing text.
  assert(cursor < items_length);

  // An unterminated object yields nothing rather than a partial span.
  const char* unclosed = "[{\"a\":1";
  cursor = 0;
  assert(!nextObjectInArray(unclosed, len(unclosed), &cursor, &begin, &end));
  assert(cursor == len(unclosed));

  return 0;
}
`;

// The scanners must stay the single implementation: no caller may reintroduce a
// private copy of the object or number scan.
const callers = [
  "src/tiles/tile_renderer.cpp",
  "src/ui/weather_popup.cpp",
];
for (const caller of callers) {
  const text = readFileSync(join(repoRoot, caller), "utf8");
  assert.match(text, /#include "src\/core\/json_scan\.h"/,
    `${caller} must use the shared scanners`);
  for (const shared of ["objectSpan", "number(", "nextObjectInArray"]) {
    assert.ok(text.includes(`hometiles_json::${shared}`),
      `${caller} must call hometiles_json::${shared}`);
  }
  assert.doesNotMatch(text, /in_string = !in_string;/,
    `${caller} still carries a private brace scanner`);
}

function findCompiler() {
  const candidates = [process.env.CXX, "clang++", "g++", "c++"].filter(Boolean);
  for (const candidate of candidates) {
    const check = spawnSync(candidate, ["--version"], { encoding: "utf8" });
    if (!check.error && check.status === 0) return candidate;
  }
  throw new Error("No C++ compiler found for the JSON scan test");
}

try {
  writeFileSync(sourcePath, source, "utf8");
  const compiler = findCompiler();
  const compile = spawnSync(
    compiler,
    ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", repoRoot,
      sourcePath, "-o", outputPath],
    { encoding: "utf8" },
  );
  assert.equal(
    compile.status,
    0,
    `JSON scan test did not compile:\n${compile.stdout}${compile.stderr}`,
  );

  const run = spawnSync(outputPath, [], { encoding: "utf8" });
  assert.equal(
    run.status,
    0,
    `JSON scan regression failed:\n${run.stdout}${run.stderr}`,
  );
  console.log("JSON payload scan regression passed");
} finally {
  rmSync(tempRoot, { recursive: true, force: true });
}
