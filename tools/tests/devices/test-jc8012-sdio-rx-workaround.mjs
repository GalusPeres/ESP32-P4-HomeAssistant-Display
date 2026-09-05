import { getBuildProfile, releaseProfiles } from '../../device-catalog.js';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const toolsDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolsDir, '../../..');
const read = (relativePath) =>
  fs
    .readFileSync(path.join(root, relativePath), 'utf8')
    .replaceAll('\r\n', '\n');
const sha256 = (relativePath) =>
  crypto
    .createHash('sha256')
    .update(fs.readFileSync(path.join(root, relativePath)))
    .digest('hex');

const variant = 'repo-guition-jc8012-rx-single-block';
const marker =
  'HomeTiles Issue30 RX single-block workaround active: max_blocks_per_CMD53=1';
const wrapper = read('src/devices/guition_jc8012p4a1/guition_jc8012p4a1_sdio.cpp');
const localBuild = read('tools/build-firmware-local.ps1');
const patchInstaller = read('tools/apply-esp-hosted-3.3.7-fixes-local.ps1');
const workflow = read('.github/workflows/firmware.yml');
const sourcePatch = read(
  'tools/esp-hosted-3.3.7-rx-fix/guition-jc8012-rx-single-block.patch',
);

assert.match(wrapper, /#if defined\(DEVICE_GUITION_JC8012P4A1\)/);
assert.doesNotMatch(wrapper, /DEVICE_GUITION_JC8012P4A1_(?:FAMILY|V2)/);
assert.match(wrapper, /config\.clock_freq_khz = 40000;/);
assert.match(wrapper, /config\.bus_width = 1;/);
assert.match(wrapper, /__wrap_esp_hosted_get_default_sdio_config/);

assert.match(localBuild, new RegExp(`'${variant.replaceAll('-', '\\-')}'`));
assert.equal(getBuildProfile('guition_jc8012p4a1').rxVariant, variant);
assert.equal(getBuildProfile('guition_jc8012p4a1').elfFlags, '-Wl,--wrap=esp_hosted_get_default_sdio_config');
assert.deepEqual(releaseProfiles.filter((profile) => profile.rxVariant === variant).map((profile) => profile.key), ['guition_jc8012p4a1']);
assert.match(localBuild, /\$buildProfile\.rxVariant/);
assert.match(localBuild, /\$elfFlags = \$buildProfile\.elfFlags/);
assert.match(localBuild, new RegExp(marker));
assert.match(
  localBuild,
  /\$resolvedEspHostedRxVariant -ne 'repo-short-tail'[\s\S]*Unexpected ESP-Hosted short-tail CMD53 RX marker/,
);

assert.match(patchInstaller, new RegExp(`'${variant.replaceAll('-', '\\-')}'`));
assert.match(patchInstaller, /\$variant -eq 'esp32p4_es-libs'/);
assert.match(patchInstaller, /'baseline-a8204'/);
assert.match(patchInstaller, /'guition-jc8012-rx-single-block'/);

const specialMatrixLines = workflow.match(
  /^\s+rx_variant: repo-guition-jc8012-rx-single-block\s*$/gm,
) ?? [];
assert.equal(
  specialMatrixLines.length,
  1,
  'Only the exact JC8012 V1 release profile may select the workaround',
);
const v1Start = workflow.indexOf('- label: Guition JC8012P4A1\n');
const v2Start = workflow.indexOf('- label: Guition JC8012P4A1 V2\n');
assert.ok(v1Start >= 0 && v2Start > v1Start);
const v1Matrix = workflow.slice(v1Start, v2Start);
assert.match(v1Matrix, /define: DEVICE_GUITION_JC8012P4A1/);
assert.match(v1Matrix, /rx_variant: repo-guition-jc8012-rx-single-block/);
assert.match(workflow, /compiler\.c\.elf\.extra_flags=\$\{elf_flags\}/);
assert.match(workflow, new RegExp(marker));

assert.match(sourcePatch, /#define H_SDIO_RX_LIMIT_XFER_SIZE_WORKDAROUND/);
assert.doesNotMatch(sourcePatch, /#define H_SDIO_TX_LIMIT_XFER_SIZE_WORKAROUND\n\+/);

assert.equal(
  sha256(
    'tools/esp-hosted-3.3.7-rx-fix/baseline-a8204/esp32p4_es-libs/port_esp_hosted_host_sdio.c.obj',
  ),
  'e6b84ebf980aa117ab73333b459d1c683a7803aee87e988592246202c5b8b598',
);
assert.equal(
  sha256(
    'tools/esp-hosted-3.3.7-rx-fix/guition-jc8012-rx-single-block/esp32p4_es-libs/port_esp_hosted_host_sdio.c.obj',
  ),
  'f3a569701af6e7483aa52778fcc5134ba90403e722b6eae002b9587973cc43ee',
);

console.log('JC8012P4A1 V1 SDIO RX workaround contract tests passed.');
