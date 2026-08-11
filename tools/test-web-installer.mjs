import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import {
  APP_SLOTS,
  DEVICE_PROFILES,
  PARTITION_TABLE,
  REQUIRED_PARTITIONS,
  assertHomeTilesPartitionLayout,
  buildFlashPlan,
  buildReleaseIndex,
  parseEspIdfPartitionTable,
  releaseAssetNames,
  resolveSameOriginAsset,
  validateFirmwareDescriptor,
} from "../docs/assets/javascripts/installer-contract.mjs";
import { selectDevicesForPublication } from "../release-helper/prepare-web-installer.mjs";

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const read = (relativePath) => fs.readFileSync(path.join(repositoryRoot, relativePath), "utf8");

function fixtureRelease(tag = "v0.6.5") {
  const assets = [];
  for (const device of DEVICE_PROFILES) {
    const names = releaseAssetNames(tag, device.key);
    for (const [mode, name] of Object.entries(names)) {
      assets.push({
        name,
        size: mode === "factory" ? device.flashSize : APP_SLOTS[0].size - 4096,
        digest: `sha256:${"a".repeat(64)}`,
        browser_download_url:
          `https://github.com/GalusPeres/HomeTiles/releases/download/${tag}/${name}`,
      });
    }
  }
  return {
    tag_name: tag,
    html_url: `https://github.com/GalusPeres/HomeTiles/releases/tag/${tag}`,
    draft: false,
    prerelease: false,
    assets,
  };
}

function writePartitionEntry(bytes, index, partition) {
  const offset = index * 32;
  const view = new DataView(bytes.buffer);
  view.setUint16(offset, 0x50aa, true);
  bytes[offset + 2] = partition.type;
  bytes[offset + 3] = partition.subtype;
  view.setUint32(offset + 4, partition.offset, true);
  view.setUint32(offset + 8, partition.size, true);
  bytes.fill(0, offset + 12, offset + 28);
  bytes.set(new TextEncoder().encode(partition.label), offset + 12);
}

function partitionTableFixture(partitions = REQUIRED_PARTITIONS) {
  const bytes = new Uint8Array(PARTITION_TABLE.size).fill(0xff);
  partitions.forEach((partition, index) => writePartitionEntry(bytes, index, partition));
  return bytes;
}

function firmwareFixture(deviceKey, appOffset = 0) {
  const bytes = new Uint8Array(appOffset + 512);
  const view = new DataView(bytes.buffer);
  bytes[appOffset] = 0xe9;
  view.setUint32(appOffset + 24 + 8, 0xabcd5432, true);
  view.setUint32(appOffset + 24 + 8 + 256, 0x44565034, true);
  bytes.set(new TextEncoder().encode(deviceKey), appOffset + 24 + 8 + 256 + 4 + 32);
  return bytes;
}

assert.equal(DEVICE_PROFILES.length, 9, "Every release target needs an installer profile.");
assert.equal(new Set(DEVICE_PROFILES.map((device) => device.key)).size, 9);
assert.equal(DEVICE_PROFILES.filter((device) => device.chipFamily === "ESP32-S3").length, 1);
assert.equal(DEVICE_PROFILES.filter((device) => device.chipFamily === "ESP32-P4").length, 8);
assert.equal(
  DEVICE_PROFILES.find((device) => device.key === "guition_esp32_4848s040").chipFamily,
  "ESP32-S3",
);

const sketchProfiles = read("sketch.yaml");
for (const device of DEVICE_PROFILES) {
  const escapedProfile = device.buildProfile.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const match = sketchProfiles.match(new RegExp(`^  ${escapedProfile}:\\r?\\n    fqbn: ([^\\r\\n]+)$`, "m"));
  assert.ok(match, `Missing sketch profile ${device.buildProfile}.`);
  const expectedBoard = device.chipFamily === "ESP32-S3" ? "esp32s3" : "esp32p4|m5stack_tab5";
  assert.match(match[1], new RegExp(`esp32:esp32:(?:${expectedBoard}):`));
  assert.match(match[1], new RegExp(`FlashSize=${device.flashSize / (1024 * 1024)}M(?:,|$)`));
}

const firmwareWorkflowKeys = [...read(".github/workflows/firmware.yml").matchAll(/^\s+key:\s+([a-z0-9_]+)\s*$/gm)]
  .map((match) => match[1]);
assert.deepEqual(
  [...DEVICE_PROFILES.map((device) => device.key)].sort(),
  [...firmwareWorkflowKeys].sort(),
  "Installer device keys must match the release build matrix.",
);

const packageSource = read("release-helper/package-ci-build.js");
const packagedDeviceKeys = [...packageSource.matchAll(/\['([a-z0-9_]+)',\s*\{\s*key:/g)]
  .map((match) => match[1]);
assert.deepEqual(
  [...DEVICE_PROFILES.map((device) => device.key)].sort(),
  packagedDeviceKeys.sort(),
  "Installer device keys must match packaged release assets.",
);
assert.match(packageSource, /const otaSlotSize = 0x680000;/);

const releaseIndex = buildReleaseIndex(fixtureRelease());
assert.equal(releaseIndex.tag, "v0.6.5");
assert.equal(releaseIndex.devices.length, 9);
for (const device of releaseIndex.devices) {
  const names = releaseAssetNames("v0.6.5", device.key);
  assert.equal(device.update.file, names.update);
  assert.equal(device.factory.file, names.factory);
  assert.equal(device.factory.size, device.flashSize);
  assert.ok(device.update.size <= APP_SLOTS[0].size);
}

const fullPublication = selectDevicesForPublication(releaseIndex);
assert.equal(fullPublication.partial, false);
assert.equal(fullPublication.devices.length, 9);
const localPublication = selectDevicesForPublication(releaseIndex, "guition_esp32_4848s040");
assert.equal(localPublication.partial, true);
assert.deepEqual(localPublication.devices.map((device) => device.key), ["guition_esp32_4848s040"]);
assert.throws(
  () => selectDevicesForPublication(releaseIndex, "unknown_device"),
  /Unknown installer device key/,
);

const missingAssetRelease = fixtureRelease();
missingAssetRelease.assets.pop();
assert.throws(() => buildReleaseIndex(missingAssetRelease), /is missing/);
const wrongFactorySizeRelease = fixtureRelease();
wrongFactorySizeRelease.assets.find((asset) => asset.name.endsWith("_factory.bin")).size -= 1;
assert.throws(() => buildReleaseIndex(wrongFactorySizeRelease), /complete .* factory image/);

const parsedPartitions = parseEspIdfPartitionTable(partitionTableFixture());
assert.equal(assertHomeTilesPartitionLayout(parsedPartitions), true);
const obsoleteLayout = REQUIRED_PARTITIONS.map((partition) =>
  partition.label === "app1" ? { ...partition, offset: 0x800000 } : partition,
);
assert.throws(
  () => assertHomeTilesPartitionLayout(parseEspIdfPartitionTable(partitionTableFixture(obsoleteLayout))),
  /app1 has an unexpected offset/,
);

const csvPartitions = read("partitions.csv")
  .split(/\r?\n/)
  .filter((line) => line.trim() && !line.trim().startsWith("#"))
  .map((line) => {
    const [label, typeName, subtypeName, offset, size] = line.split(",").map((field) => field.trim());
    const type = typeName === "app" ? 0x00 : 0x01;
    const subtypeMap = { nvs: 0x02, ota: 0x00, ota_0: 0x10, ota_1: 0x11, spiffs: 0x82, coredump: 0x03 };
    return { label, type, subtype: subtypeMap[subtypeName], offset: Number(offset), size: Number(size) };
  });
assert.equal(assertHomeTilesPartitionLayout(csvPartitions), true);

const updateFirmware = firmwareFixture("m5stacks_tab5");
assert.equal(validateFirmwareDescriptor(updateFirmware, "m5stacks_tab5"), "m5stacks_tab5");
assert.throws(() => validateFirmwareDescriptor(updateFirmware, "waveshare_4b"), /Firmware is for/);
const factoryFirmware = firmwareFixture("guition_esp32_4848s040", 0x10000);
assert.equal(
  validateFirmwareDescriptor(factoryFirmware, "guition_esp32_4848s040", 0x10000),
  "guition_esp32_4848s040",
);

assert.throws(
  () => buildFlashPlan("update", updateFirmware),
  /requires current OTA data and buildSafeOtaUpdatePlan/,
);
const factoryPlan = buildFlashPlan("factory", factoryFirmware);
assert.equal(factoryPlan.eraseFirst, true);
assert.deepEqual(factoryPlan.parts.map((part) => part.address), [0]);

assert.equal(
  resolveSameOriginAsset(
    new URL("https://galusperes.github.io/HomeTiles/firmware/latest/release.json"),
    "hometiles_v0.6.5_m5stacks_tab5.bin",
    "https://galusperes.github.io",
  ).origin,
  "https://galusperes.github.io",
);
assert.throws(
  () => resolveSameOriginAsset(
    new URL("https://github.com/GalusPeres/HomeTiles/releases/download/v0.6.5/release.json"),
    "hometiles_v0.6.5_m5stacks_tab5.bin",
    "https://galusperes.github.io",
  ),
  /documentation origin/,
);

const docsWorkflow = read(".github/workflows/docs.yml");
assert.match(docsWorkflow, /push:\s*\n\s+branches:\s*\[master\]/);
assert.match(docsWorkflow, /workflow_dispatch:/);
assert.doesNotMatch(
  docsWorkflow,
  /release:\s*\n\s+types:\s*\[published\]/,
  "A release created with GITHUB_TOKEN does not reliably trigger another workflow.",
);
assert.match(docsWorkflow, /node tools\/test-web-installer\.mjs/);
assert.match(docsWorkflow, /mkdocs build --strict/);
assert.match(
  docsWorkflow,
  /prepare-web-installer\.mjs --output site\/firmware\/latest/,
  "Release assets must be copied into the deployed same-origin site.",
);
assert.match(docsWorkflow, /ghp-import .*--no-history.* site/);
assert.doesNotMatch(docsWorkflow, /mkdocs gh-deploy/);

const firmwareWorkflow = read(".github/workflows/firmware.yml");
assert.match(firmwareWorkflow, /Verify browser installer contract/);
assert.match(firmwareWorkflow, /node tools\/test-web-installer\.mjs/);
assert.match(
  firmwareWorkflow,
  /release:[\s\S]*?permissions:\s*\n\s+actions:\s*write\s*\n\s+contents:\s*write/,
  "The release job needs Actions write permission to dispatch the docs workflow.",
);
const releaseUploadIndex = firmwareWorkflow.indexOf("gh release upload");
const docsDispatchIndex = firmwareWorkflow.indexOf("gh workflow run docs.yml");
assert.ok(releaseUploadIndex >= 0, "The release workflow must upload firmware assets.");
assert.ok(
  docsDispatchIndex > releaseUploadIndex,
  "Docs must be dispatched only after all release assets were uploaded.",
);
assert.match(
  firmwareWorkflow.slice(docsDispatchIndex),
  /--repo "\$GITHUB_REPOSITORY"[\s\\]*\n\s+--ref "\$GITHUB_REF_NAME"/,
  "The docs workflow must run for the just-published release tag.",
);

const installerSource = read("docs/assets/javascripts/installer.mjs");
assert.doesNotMatch(installerSource, /github\.com\/GalusPeres\/HomeTiles\/releases\/download/);
assert.match(installerSource, /resolveSameOriginAsset/);
assert.match(installerSource, /verifyExistingLayout/);
assert.match(installerSource, /eraseFlash\(\)/);
assert.match(installerSource, /populateDevices\(index\.devices\)/);
assert.match(installerSource, /function selectedFirmwareSource\(\)/);
assert.match(installerSource, /elements\.firmwareSource\.options\[0\]\.textContent = `Published release \$\{index\.tag\}`/);
assert.match(installerSource, /elements\.firmwareFile\.files\[0\]/);
assert.match(installerSource, /Local Update image has \$\{bytes\.length\} bytes/);
assert.match(installerSource, /Local Factory image has \$\{bytes\.length\} bytes/);
assert.match(installerSource, /validateFirmwareDescriptor\(bytes, device\.key, mode === "factory" \? 0x10000 : 0\)/);
assert.match(installerSource, /elements\.exactHardware\.checked = false/);
assert.match(
  installerSource,
  /const safeToReset = completed \|\| !flashMutationStarted \|\| mode === "update";/,
  "An interrupted inactive-slot Update may reboot the preserved selected slot.",
);
assert.match(installerSource, /const GUITION_S3_DEVICE_KEY = "guition_esp32_4848s040"/);
assert.match(
  installerSource,
  /const GUITION_S3_NORMAL_BOOT_RESET_SEQUENCE = "D0\|R1\|W100\|R0\|W100\|D0"/,
);
assert.match(installerSource, /device\?\.key === GUITION_S3_DEVICE_KEY/);
assert.match(
  installerSource,
  /esploader\.after\("custom_reset", undefined, GUITION_S3_NORMAL_BOOT_RESET_SEQUENCE\)/,
);
assert.match(installerSource, /resetAndDisconnect\(esploader, transport, safeToReset, device\)/);
assert.doesNotMatch(installerSource, /display is restarting/);
assert.match(installerSource, /Update complete\. Settings were preserved\. Please restart the device manually\./);
assert.match(installerSource, /Factory reset complete\. Local settings were erased\. Please restart the device manually\./);
assert.doesNotMatch(installerSource, /reset signal was sent|Power-cycle once if/);
assert.match(installerSource, /LAST_RUN_STORAGE_KEY = "hometiles\.webInstaller\.lastRun\.v1"/);
assert.match(installerSource, /function restoreLastRun\(\)/);
assert.match(installerSource, /"Recovery required"/);
assert.match(installerSource, /The previously selected app slot was preserved/);
assert.match(installerSource, /window\.addEventListener\("beforeunload"/);
assert.match(installerSource, /event\.preventDefault\(\)/);
assert.match(installerSource, /window\.addEventListener\("pagehide"/);
assert.match(installerSource, /elements\.progressText\.textContent = `\$\{Math\.round\(state\.progress\)\}%`/);
assert.match(installerSource, /buildSafeOtaUpdatePlan\(firmware, otaData\)/);
assert.match(installerSource, /`Writing inactive slot \$\{targetLabel\}`/);
assert.match(installerSource, /baudrate: INSTALLER_BAUD_RATE/);
assert.match(installerSource, /const LOG_MAX_LINES = 300/);
assert.match(installerSource, /const LOG_MAX_CHARACTERS = 64 \* 1024/);
assert.match(installerSource, /\[\$\{source\}\]/);
assert.match(installerSource, /terminalPartial/);
assert.match(installerSource, /navigator\.clipboard\?\.writeText/);
assert.doesNotMatch(installerSource, /URL\.createObjectURL|downloadFlashLog|installer-download-log/);
assert.match(installerSource, /logState\.followTail = distanceFromBottom <= 8/);
const flashSelectedSource = installerSource.slice(installerSource.indexOf("async function flashSelectedFirmware"));
assert.ok(
  flashSelectedSource.indexOf("await downloadFirmware(device, mode)") <
    flashSelectedSource.indexOf("await navigator.serial.requestPort()"),
  "Firmware must download and verify before the browser touches the serial device.",
);

const installerDocs = read("docs/installer.md");
assert.match(installerDocs, /class="ht-installer-form"/);
assert.match(installerDocs, /id="installer-firmware-source"/);
assert.match(installerDocs, /Local HomeTiles \.bin file/);
assert.match(installerDocs, /id="installer-firmware-file"[^>]*type="file"/);
assert.match(installerDocs, /<label id="installer-device-label" class="ht-installer-section-heading" for="installer-device">2\. Device<\/label>/);
for (const label of ["1. Firmware", "3. Flash mode", "4. Confirm", "5. Connect and flash"]) {
  assert.match(installerDocs, new RegExp(label.replace(".", "\\.")));
}
assert.match(installerDocs, /<strong>Update<\/strong>/);
assert.match(installerDocs, /<strong>First install \/ factory reset<\/strong>/);
assert.match(installerDocs, /id="installer-progress-panel" class="ht-installer-progress" hidden/);
assert.match(installerDocs, /id="installer-phase"/);
assert.match(installerDocs, /id="installer-progress-text">0%/);
assert.match(installerDocs, /id="installer-log-panel" class="ht-installer-log-panel" hidden/);
assert.match(installerDocs, /aria-label="Copy flash log to clipboard"/);
assert.doesNotMatch(installerDocs, /Download log|Show flash log|installer-download-log|installer-log-details/);
assert.match(installerDocs, /role="log" aria-label="Installer flash log"/);
assert.doesNotMatch(installerDocs, /writes both application slots|Writing both app slots|cover both slots/);
assert.doesNotMatch(installerDocs, /ht-installer-(?:grid|step|step-number|heading|kicker)/);
assert.doesNotMatch(installerDocs, /<strong>Update\s+—/);
assert.doesNotMatch(installerDocs, /<strong>Factory\s*\//);
assert.doesNotMatch(installerDocs, /Lokaler|Veröffentlichung|Gerät auswählen|gültigen Werte/);
assert.match(installerDocs, /Select the exact model printed on the device or rear label/);
assert.doesNotMatch(installerDocs, /Choose the right mode|\| Mode \| Use it for \| Local data \|/);
for (const heading of [
  "## Browser installer",
  "## Update HomeTiles",
  "## First install or factory reset",
  "## Manual flashing",
  "## Troubleshooting",
]) {
  assert.match(installerDocs, new RegExp(`^${heading.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}$`, "m"));
}
assert.match(installerDocs, /Under \*\*Firmware\*\*, use the published release/);
assert.match(installerDocs, /Settings → System/);
assert.match(installerDocs, /Update safety and partition details/);
assert.match(installerDocs, /The installer writes and verifies only the inactive application slot/);
assert.match(installerDocs, /Boot entry committed after app verification/);
assert.match(installerDocs, /previously selected app remains available/);
assert.match(installerDocs, /## Manual flashing/);
assert.match(installerDocs, /manual flashing guide/);
assert.match(installerDocs, /<details class="ht-installer-log ht-installer-doc-details" markdown="1">/);
assert.doesNotMatch(installerDocs, /<details class="ht-installer-log ht-installer-doc-details"[^>]*\sopen(?:\s|>)/);
assert.match(installerDocs, /Local test before publication/);
assert.match(installerDocs, /--device guition_esp32_4848s040/);
assert.match(installerDocs, /http:\/\/127\.0\.0\.1:8000\/installer\//);
assert.match(installerDocs, /SHA-256-verified assets/);

const overviewDocs = read("docs/index.md");
assert.match(overviewDocs, /\*\*Flashing the Firmware\*\*/);
assert.match(overviewDocs, /Install or update HomeTiles directly here in the browser/);
assert.match(overviewDocs, /\[Flashing the Firmware :octicons-arrow-right-24:\]\(installer\.md\)/);
assert.doesNotMatch(
  overviewDocs,
  /Open the Browser Installer|Every normal update after that|Manual flashing instructions/,
);

const updatingDocs = read("docs/updating.md");
assert.match(updatingDocs, /only to the inactive application slot/);
assert.match(updatingDocs, /committing a new redundant OTA selection entry/);
assert.match(updatingDocs, /previously selected application remains untouched and bootable/);

const installerStyles = read("docs/stylesheets/extra.css");
const installerStyleStart = installerStyles.indexOf("Browser firmware installer");
assert.ok(installerStyleStart >= 0);
const installerStylesOnly = installerStyles.slice(installerStyleStart);
assert.match(installerStylesOnly, /\.ht-installer\s*\{[\s\S]*?width:\s*100%/);
assert.match(installerStylesOnly, /grid-template-columns:\s*7\.5rem minmax\(0, 1fr\)/);
assert.match(installerStylesOnly, /\.ht-installer-section \+ \.ht-installer-section\s*\{[\s\S]*?border-top:\s*1px solid/);
assert.match(installerStylesOnly, /\.ht-installer select\s*\{[\s\S]*?width:\s*15rem/);
assert.match(installerStylesOnly, /\.ht-installer select\s*\{[\s\S]*?appearance:\s*none/);
assert.match(installerStylesOnly, /background-position:\s*right 0\.38rem center/);
assert.match(installerStylesOnly, /\.ht-installer-section-info\s*\{[\s\S]*?color:\s*var\(--md-default-fg-color--light\)/);
assert.match(installerStylesOnly, /\.ht-installer-action button\s*\{[\s\S]*?white-space:\s*nowrap/);
assert.doesNotMatch(installerStylesOnly, /radial-gradient|ht-installer-grid|ht-installer-step-number/);
assert.match(installerStylesOnly, /\.ht-installer-log-panel pre\s*\{[\s\S]*?max-height:\s*14rem/);

const docsNavigation = read("mkdocs.yml");
assert.equal(
  [...docsNavigation.matchAll(/^\s+- Flashing the Firmware:\s*([^\r\n]+)$/gm)].length,
  1,
  "The sidebar must contain one clear flashing entry.",
);
assert.match(docsNavigation, /- Flashing the Firmware: installer\.md/);
assert.doesNotMatch(docsNavigation, /Browser Firmware Installer:/);
assert.match(
  docsNavigation,
  /- Release Notes:\s*\r?\n\s+- v0\.6\.6:\s*releases\/v0\.6\.6\.md\s*\r?\n\s+- v0\.6\.5:/,
);
assert.doesNotMatch(docsNavigation, /navigation\.expand/);
for (const match of docsNavigation.matchAll(/^\s+- (?:[^:\r\n]+):\s+([^\s#]+\.md)\s*$/gm)) {
  assert.ok(fs.existsSync(path.join(repositoryRoot, "docs", match[1])), `Missing nav target ${match[1]}.`);
}

const screensaverDocs = read("docs/screensaver.md");
assert.match(screensaverDocs, /baseline \(non-progressive\) JPEG/);
assert.match(screensaverDocs, /RGB\/sRGB/);
assert.match(screensaverDocs, /longest edge at \*\*1920 pixels/);
for (const [target, resolution] of [
  ["M5Stack Tab5", "1280×720"],
  ["Waveshare 4B / 86 Panel", "720×720"],
  ["Waveshare Touch LCD 7 inch", "1280×720"],
  ["Waveshare Touch LCD 8 inch", "1280×800"],
  ["Waveshare Touch LCD 10.1 inch", "1280×800"],
  ["Guition JC8012P4A1 V1 / V2", "1280×800"],
  ["Guition JC1060P470C_I_W_Y", "1024×600"],
  ["Guition ESP32-4848S040C_I", "480×480"],
]) {
  assert.ok(
    screensaverDocs.includes(`| ${target} | ${resolution} |`),
    `Missing screensaver image guidance for ${target}.`,
  );
}

console.log("Browser installer contract tests passed.");
