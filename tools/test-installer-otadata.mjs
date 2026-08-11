import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import {
  APP_SLOTS,
  ESP_OTA_IMAGE_STATE,
  INSTALLER_BAUD_RATE,
  OTA_DATA_LAYOUT,
  assertOtaBootSelectionApplied,
  buildOtaBootSelectionUpdate,
  buildSafeOtaUpdatePlan,
  espIdfOtaSelectCrc,
  parseEspIdfOtaData,
} from "../docs/assets/javascripts/installer-contract.mjs";

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

function blankOtaData() {
  return new Uint8Array(OTA_DATA_LAYOUT.size).fill(0xff);
}

function writeU32(bytes, offset, value) {
  new DataView(bytes.buffer, bytes.byteOffset + offset, 4).setUint32(0, value, true);
}

function writeEntry(
  otaData,
  entryIndex,
  { sequence, state = ESP_OTA_IMAGE_STATE.UNDEFINED, crc = espIdfOtaSelectCrc(sequence), labelByte = 0xff },
) {
  const offset = OTA_DATA_LAYOUT.entryOffsets[entryIndex];
  otaData.fill(0xff, offset, offset + OTA_DATA_LAYOUT.sectorSize);
  writeU32(otaData, offset, sequence);
  otaData.fill(labelByte, offset + 4, offset + 24);
  writeU32(otaData, offset + 24, state);
  writeU32(otaData, offset + 28, crc);
  return otaData;
}

function applyBootSelectionUpdate(otaData, update) {
  const result = new Uint8Array(otaData);
  const relativeOffset = update.address - OTA_DATA_LAYOUT.offset;
  result.fill(0xff, relativeOffset, relativeOffset + update.sectorSize);
  result.set(update.data, relativeOffset);
  return result;
}

function hex(bytes) {
  return Buffer.from(bytes).toString("hex");
}

assert.equal(INSTALLER_BAUD_RATE, 460800);
assert.equal(OTA_DATA_LAYOUT.offset, 0xe000);
assert.equal(OTA_DATA_LAYOUT.size, 0x2000);
assert.equal(OTA_DATA_LAYOUT.sectorSize, 0x1000);
assert.deepEqual(OTA_DATA_LAYOUT.entryOffsets, [0, 0x1000]);
assert.equal(APP_SLOTS.length, 2);

// These values match esp_rom_crc32_le(UINT32_MAX, &ota_seq, 4) in ESP-IDF v5.5.2.
assert.equal(espIdfOtaSelectCrc(0), 0xffffffff);
assert.equal(espIdfOtaSelectCrc(1), 0x4743989a);
assert.equal(espIdfOtaSelectCrc(2), 0x55f63774);
assert.equal(espIdfOtaSelectCrc(3), 0xed4a5011);
assert.equal(espIdfOtaSelectCrc(0xfffffffe), 0x99f8b879);

// Actual first otadata entry from the published v0.6.5 Guition S3 factory image.
const factoryEntry = Buffer.from(
  "01000000" + "ff".repeat(20) + "ffffffff" + "9a984347",
  "hex",
);
assert.equal(factoryEntry.length, OTA_DATA_LAYOUT.entrySize);
const factoryOtaData = blankOtaData();
factoryOtaData.set(factoryEntry, 0);

const localFactoryPath = path.join(
  repositoryRoot,
  "site",
  "firmware",
  "latest",
  "hometiles_v0.6.5_guition_esp32_4848s040_factory.bin",
);
if (fs.existsSync(localFactoryPath)) {
  const publishedFactory = fs.readFileSync(localFactoryPath);
  assert.equal(publishedFactory[APP_SLOTS[0].offset], 0xe9);
  assert.ok(
    publishedFactory.subarray(APP_SLOTS[1].offset, APP_SLOTS[1].offset + 32).every((byte) => byte === 0xff),
    "The published factory vector is expected to contain only app0.",
  );
  assert.equal(
    hex(publishedFactory.subarray(OTA_DATA_LAYOUT.offset, OTA_DATA_LAYOUT.offset + 32)),
    hex(factoryEntry),
  );
}

const factorySelection = parseEspIdfOtaData(factoryOtaData);
assert.equal(factorySelection.activeEntryIndex, 0);
assert.equal(factorySelection.selectedSlotIndex, 0);
assert.equal(factorySelection.selectionSource, "otadata");
assert.equal(factorySelection.entries[0].otaSequence, 1);
assert.equal(factorySelection.entries[0].otaState, ESP_OTA_IMAGE_STATE.UNDEFINED);
assert.equal(factorySelection.entries[0].crcValid, true);
assert.equal(factorySelection.entries[0].valid, true);
assert.equal(factorySelection.entries[1].erased, true);
assert.equal(factorySelection.entries[1].valid, false);

const firmware = new Uint8Array([0xe9, 0x01, 0x02, 0x03]);
const firstPlan = buildSafeOtaUpdatePlan(firmware, factoryOtaData);
assert.equal(firstPlan.preservedSlotIndex, 0);
assert.equal(firstPlan.targetSlotIndex, 1);
assert.equal(firstPlan.appWrite.address, APP_SLOTS[1].offset);
assert.equal(firstPlan.appWrite.data, firmware);
assert.equal(firstPlan.bootSelectionWrite.address, 0xf000);
assert.equal(firstPlan.bootSelectionWrite.writeEntryIndex, 1);
assert.equal(firstPlan.bootSelectionWrite.otaSequence, 2);
assert.equal(firstPlan.bootSelectionWrite.otaState, ESP_OTA_IMAGE_STATE.NEW);
assert.equal(firstPlan.bootSelectionWrite.sectorSize, 0x1000);
assert.equal(firstPlan.bootSelectionWrite.data.length, 32);
assert.equal(
  hex(firstPlan.bootSelectionWrite.data),
  "02000000" + "ff".repeat(20) + "00000000" + "7437f655",
);
assert.equal("parts" in firstPlan, false, "The app and boot selection must be separate verified writes.");

const afterFirstUpdate = applyBootSelectionUpdate(factoryOtaData, firstPlan.bootSelectionWrite);
assert.equal(assertOtaBootSelectionApplied(afterFirstUpdate, firstPlan.bootSelectionWrite), true);
const firstUpdatedSelection = parseEspIdfOtaData(afterFirstUpdate);
assert.equal(firstUpdatedSelection.activeEntryIndex, 1);
assert.equal(firstUpdatedSelection.selectedSlotIndex, 1);

const secondPlan = buildSafeOtaUpdatePlan(firmware, afterFirstUpdate);
assert.equal(secondPlan.preservedSlotIndex, 1);
assert.equal(secondPlan.targetSlotIndex, 0);
assert.equal(secondPlan.appWrite.address, APP_SLOTS[0].offset);
assert.equal(secondPlan.bootSelectionWrite.address, 0xe000);
assert.equal(secondPlan.bootSelectionWrite.writeEntryIndex, 0);
assert.equal(secondPlan.bootSelectionWrite.otaSequence, 3);
assert.equal(
  hex(secondPlan.bootSelectionWrite.data),
  "03000000" + "ff".repeat(20) + "00000000" + "11504aed",
);

const afterSecondUpdate = applyBootSelectionUpdate(afterFirstUpdate, secondPlan.bootSelectionWrite);
assert.equal(assertOtaBootSelectionApplied(afterSecondUpdate, secondPlan.bootSelectionWrite), true);
assert.equal(parseEspIdfOtaData(afterSecondUpdate).selectedSlotIndex, 0);

const erasedSelection = parseEspIdfOtaData(blankOtaData());
assert.equal(erasedSelection.activeEntryIndex, null);
assert.equal(erasedSelection.selectedSlotIndex, 0);
assert.equal(erasedSelection.selectionSource, "default-ota-0");
const erasedPlan = buildSafeOtaUpdatePlan(firmware, blankOtaData());
assert.equal(erasedPlan.targetSlotIndex, 1);
assert.equal(erasedPlan.bootSelectionWrite.writeEntryIndex, 0);
assert.equal(erasedPlan.bootSelectionWrite.otaSequence, 2);

const onlySecondValid = blankOtaData();
writeEntry(onlySecondValid, 1, { sequence: 2, state: ESP_OTA_IMAGE_STATE.VALID });
assert.equal(parseEspIdfOtaData(onlySecondValid).activeEntryIndex, 1);
assert.equal(parseEspIdfOtaData(onlySecondValid).selectedSlotIndex, 1);

const bothValid = blankOtaData();
writeEntry(bothValid, 0, { sequence: 3, state: ESP_OTA_IMAGE_STATE.VALID });
writeEntry(bothValid, 1, { sequence: 4, state: ESP_OTA_IMAGE_STATE.PENDING_VERIFY });
assert.equal(parseEspIdfOtaData(bothValid).activeEntryIndex, 1);
assert.equal(parseEspIdfOtaData(bothValid).selectedSlotIndex, 1);
writeEntry(bothValid, 0, { sequence: 4, state: ESP_OTA_IMAGE_STATE.VALID });
assert.equal(parseEspIdfOtaData(bothValid).activeEntryIndex, 0, "Equal sequences select entry zero.");

const badCrc = blankOtaData();
writeEntry(badCrc, 0, { sequence: 1 });
writeEntry(badCrc, 1, { sequence: 2, crc: 0x12345678 });
assert.equal(parseEspIdfOtaData(badCrc).activeEntryIndex, 0);
assert.equal(parseEspIdfOtaData(badCrc).entries[1].crcValid, false);
assert.equal(parseEspIdfOtaData(badCrc).entries[1].valid, false);

for (const invalidState of [ESP_OTA_IMAGE_STATE.INVALID, ESP_OTA_IMAGE_STATE.ABORTED]) {
  const invalid = blankOtaData();
  writeEntry(invalid, 0, { sequence: 1, state: invalidState });
  assert.equal(parseEspIdfOtaData(invalid).activeEntryIndex, null);
  assert.equal(parseEspIdfOtaData(invalid).entries[0].crcValid, true);
  assert.equal(parseEspIdfOtaData(invalid).entries[0].valid, false);
}

const unknownState = blankOtaData();
writeEntry(unknownState, 0, { sequence: 1, state: 0x12345678 });
assert.equal(
  parseEspIdfOtaData(unknownState).entries[0].valid,
  true,
  "ESP-IDF invalidates only INVALID and ABORTED states.",
);

const zeroSequence = blankOtaData();
writeEntry(zeroSequence, 0, { sequence: 0 });
assert.equal(parseEspIdfOtaData(zeroSequence).entries[0].valid, true);
assert.equal(parseEspIdfOtaData(zeroSequence).selectedSlotIndex, 1);

const preservedLabel = blankOtaData();
writeEntry(preservedLabel, 0, { sequence: 1 });
writeEntry(preservedLabel, 1, { sequence: 0, state: ESP_OTA_IMAGE_STATE.INVALID, labelByte: 0xa5 });
const preservedLabelUpdate = buildOtaBootSelectionUpdate(preservedLabel, 1);
assert.equal(hex(preservedLabelUpdate.data.subarray(4, 24)), "a5".repeat(20));

const exhausted = blankOtaData();
writeEntry(exhausted, 0, { sequence: 0xfffffffe });
assert.throws(
  () => buildSafeOtaUpdatePlan(firmware, exhausted),
  /sequence space is exhausted/,
);

assert.throws(() => parseEspIdfOtaData(new Uint8Array(31)), /read is too short/);
assert.throws(() => buildSafeOtaUpdatePlan(new Uint8Array(), factoryOtaData), /image is empty/);
assert.throws(
  () => buildSafeOtaUpdatePlan(new Uint8Array(APP_SLOTS[0].size + 1), factoryOtaData),
  /exceeds a HomeTiles app slot/,
);

const tamperedReadback = new Uint8Array(afterFirstUpdate);
tamperedReadback[OTA_DATA_LAYOUT.entryOffsets[1] + 4] ^= 0x01;
assert.throws(
  () => assertOtaBootSelectionApplied(tamperedReadback, firstPlan.bootSelectionWrite),
  /read-back verification/,
);

console.log("Browser installer OTA data contract tests passed.");
