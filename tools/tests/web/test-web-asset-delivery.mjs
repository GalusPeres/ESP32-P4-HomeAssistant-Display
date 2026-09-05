import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {gzipSync, gunzipSync} from 'node:zlib';
import {Script, runInNewContext} from 'node:vm';
import {fileURLToPath} from 'node:url';
import {readAdminBundle} from '../../lib/admin-bundle.mjs';
import {assertAdminDeliveryEquivalent, formatAdminDelivery} from '../../lib/admin-delivery.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const read = relative => fs.readFileSync(path.join(root, relative), 'utf8').replace(/\r\n?/g, '\n');

assertAdminDeliveryEquivalent('const name = 0x10; const value = {name: name};',
  'const name=16;const value={name};');
assertAdminDeliveryEquivalent('const value = "a";', 'const value="\\x61";');

const rejected = [
  ['const value = 1;', 'const value = 2;'],
  ['function handleClick() {}', 'function a() {}'],
  ['if (enabled) notify();', 'notify();'],
  ['const value = {__proto__};', 'const value = {__proto__: __proto__};'],
  ['const value = {[name]: name};', 'const value = {name};'],
  ['const value = {get name() { return name; }};', 'const value = {name};'],
  ['const value = {name() { return name; }};', 'const value = {name};'],
  ['const {name} = value;', 'const {name: name} = value;'],
  // Tagged templates observe raw text even when their cooked values are equal.
  ['const value = String.raw`\\x61`;', 'const value = String.raw`a`;'],
  ['const value = /first/gi;', 'const value = /second/gi;'],
];
for (const [source, delivery] of rejected) {
  assert.throws(() => assertAdminDeliveryEquivalent(source, delivery), /Admin delivery AST changed/);
}
assert.throws(() => assertAdminDeliveryEquivalent('const value = 1;', 'const = ;'), SyntaxError);

const fixture = `
/*! @license Preserve this license. */
// Remove this ordinary delivery comment.
const label = 'ready';
const state = {label: label, count: 0};
const events = [];
function handleClick(value) { events.push(value); state.count++; return state.label; }
globalThis.result = [handleClick(3), events, Object.keys(state), state.count];
`;
const fixtureDelivery = await formatAdminDelivery(fixture);
assert.match(fixtureDelivery, /Preserve this license/);
assert.doesNotMatch(fixtureDelivery, /Remove this ordinary delivery comment/);
assert.match(fixtureDelivery, /handleClick/);
assert.equal(JSON.stringify(runInNewContext(fixture + '\nresult;')),
  JSON.stringify(runInNewContext(fixtureDelivery + '\nresult;')));

const bundle = readAdminBundle(root);
assert.equal(read('src/web/assets/admin.js'), bundle.source,
  'The assembled JavaScript must remain readable and identical to its source units');
const delivery = await formatAdminDelivery(bundle.source);
assert.equal(await formatAdminDelivery(bundle.source), delivery, 'Delivery formatting must be deterministic');
new Script(delivery, {filename: 'admin.delivery.js'});
const compressed = Buffer.from([...read('src/web/generated/admin_js_gzip.inc').matchAll(/0x([0-9a-f]{2})/g)]
  .map(match => parseInt(match[1], 16)));
assert.equal(gunzipSync(compressed).toString('utf8'), delivery,
  'The firmware must serve the checked delivery output');
const expectedGzip = gzipSync(Buffer.from(delivery), {level: 9, mtime: 0});
expectedGzip[9] = 255;
assert.deepEqual(compressed, expectedGzip);
assert.equal(compressed[9], 255, 'Normalize the gzip OS marker across hosts');
assert.ok(compressed.length < gzipSync(Buffer.from(bundle.source), {level: 9}).length,
  'Delivery formatting should reduce the embedded JavaScript asset');

const meta = read('src/web/generated/admin_assets_meta.h');
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
assert.ok(meta.includes(`/assets/admin.${hash(delivery).slice(0, 12)}.js`),
  'The immutable URL must identify decoded delivery bytes');
assert.ok(meta.includes(hash(compressed)), 'The ETag must identify the compressed representation');
assert.match(meta, new RegExp(`kAdminJsSourceSize = ${Buffer.byteLength(delivery)};`));
assert.match(meta, new RegExp(`kAdminJsGzipSize = ${compressed.length};`));

const manifest = JSON.parse(read('package.json'));
const lock = JSON.parse(read('package-lock.json'));
assert.equal(manifest.private, true);
assert.notEqual(manifest.type, 'module', 'Existing CommonJS host tools must retain their module type');
for (const name of ['terser', 'acorn']) {
  assert.match(manifest.devDependencies[name], /^\d+\.\d+\.\d+$/, `${name} must be pinned exactly`);
  assert.equal(lock.packages[`node_modules/${name}`].version, manifest.devDependencies[name]);
}
let checkedJobs = 0;
for (const filename of fs.readdirSync(path.join(root, '.github/workflows')).filter(name => /\.ya?ml$/.test(name))) {
  const workflow = read(`.github/workflows/${filename}`);
  for (const job of workflow.split(/(?=^  [A-Za-z_][A-Za-z0-9_-]*:\s*$)/m)) {
    const command = job.search(/node tools\/(?:generate-web-assets\.mjs|run-tests\.mjs|tests\/)/);
    if (command < 0) continue;
    const install = job.indexOf('npm ci --ignore-scripts');
    assert.ok(install >= 0 && install < command, `${filename} must install pinned tooling before generation/tests`);
    checkedJobs++;
  }
}
assert.ok(checkedJobs > 0, 'Exercise the dependency setup in the actual CI jobs');
console.log('Web asset delivery formatting, AST guard, gzip identity and CI dependency setup passed.');
