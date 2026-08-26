import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const repoRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const tempRoot = mkdtempSync(join(tmpdir(), "hometiles-mqtt-safety-"));
const sourcePath = join(tempRoot, "mqtt_packet_safety_test.cpp");
const outputPath = join(
  tempRoot,
  process.platform === "win32" ? "mqtt_packet_safety_test.exe" : "mqtt_packet_safety_test",
);

const source = String.raw`
#include "src/network/mqtt_packet_safety.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

int main() {
  using namespace hometiles_mqtt;

  assert(packetFitsBuffer(2, 50, 52));
  assert(!packetFitsBuffer(2, 51, 52));
  assert(!packetFitsBuffer(5, 268435455U, 32768));

  // Exact values recovered from the v0.6.8 Guition S3 core dump: a 52-byte
  // packet claimed a 13,869-byte topic and previously wrapped the callback
  // payload length to 0xffffca03.
  uint8_t malformed[52] = {};
  malformed[0] = 0x30;
  malformed[1] = 0x32;
  malformed[2] = 0x36;
  malformed[3] = 0x2d;
  PublishPacketLayout layout;
  assert(!publishRemainingLengthIsValid(50, 13869, kQos0));
  assert(!computePublishPacketLayout(
      malformed, sizeof(malformed), 32768, 1, &layout));

  uint8_t qos0[] = {0x30, 0x04, 0x00, 0x01, 'a', 'x'};
  assert(computePublishPacketLayout(
      qos0, sizeof(qos0), sizeof(qos0), 1, &layout));
  assert(layout.topic_source_offset == 4);
  assert(layout.topic_callback_offset == 3);
  assert(layout.topic_length == 1);
  assert(layout.payload_offset == 5);
  assert(layout.payload_length == 1);
  assert(layout.qos_bits == kQos0);

  uint8_t qos1[] = {0x32, 0x06, 0x00, 0x01, 'a', 0x12, 0x34, 'x'};
  assert(computePublishPacketLayout(
      qos1, sizeof(qos1), sizeof(qos1), 1, &layout));
  assert(layout.packet_id_offset == 5);
  assert(layout.payload_offset == 7);
  assert(layout.payload_length == 1);
  assert(layout.qos_bits == kQos1);

  uint8_t empty_topic[] = {0x30, 0x02, 0x00, 0x00};
  assert(!computePublishPacketLayout(
      empty_topic, sizeof(empty_topic), sizeof(empty_topic), 1, &layout));

  uint8_t unsupported_qos2[] = {0x34, 0x05, 0x00, 0x01, 'a', 0x00, 0x01};
  assert(!computePublishPacketLayout(
      unsupported_qos2, sizeof(unsupported_qos2),
      sizeof(unsupported_qos2), 1, &layout));

  size_t total = 123;
  assert(!checkedInboundAllocationSize(
      12, 13869, static_cast<size_t>(0xffffca03U), 32768, &total));
  assert(total == 0);
  assert(checkedInboundAllocationSize(12, 10, 20, 1024, &total));
  assert(total == 43);
  assert(!checkedInboundAllocationSize(12, 900, 200, 1024, &total));
  assert(!checkedInboundAllocationSize(SIZE_MAX, 1, 0, SIZE_MAX, &total));

  return 0;
}
`;

const pubSubClient = readFileSync(
  join(repoRoot, "src/network/vendor/pubsubclient/PubSubClient.cpp"),
  "utf8",
);
const mqttHandlers = readFileSync(
  join(repoRoot, "src/network/mqtt_handlers.cpp"),
  "utf8",
);
assert.match(pubSubClient, /publishRemainingLengthIsValid\s*\(/);
assert.match(pubSubClient, /computePublishPacketLayout\s*\(/);
assert.match(pubSubClient, /packetFitsBuffer\s*\(/);
assert.match(pubSubClient, /abortPacket\(MQTT_CONNECTION_TIMEOUT\)/);
assert.match(pubSubClient, /MQTT_MALFORMED_PACKET/);
assert.match(mqttHandlers, /checkedInboundAllocationSize\s*\(/);
assert.match(mqttHandlers, /strnlen\(topic, topic_scan_limit\)/);

function findCompiler() {
  const candidates = [process.env.CXX, "clang++", "g++", "c++"].filter(Boolean);
  for (const candidate of candidates) {
    const check = spawnSync(candidate, ["--version"], { encoding: "utf8" });
    if (!check.error && check.status === 0) return candidate;
  }
  throw new Error("No C++ compiler found for the MQTT packet safety test");
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
    `MQTT packet safety test did not compile:\n${compile.stdout}${compile.stderr}`,
  );

  const run = spawnSync(outputPath, [], { encoding: "utf8" });
  assert.equal(
    run.status,
    0,
    `MQTT packet safety regression failed:\n${run.stdout}${run.stderr}`,
  );
  console.log("MQTT packet safety regression passed");
} finally {
  rmSync(tempRoot, { recursive: true, force: true });
}
