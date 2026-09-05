import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { getBuildProfile } from './device-catalog.js';
import { parseFirmwareMetadata, assertReleaseMetadata } from '../release-helper/firmware-metadata.js';

function inspectFirmware(directory, profile) {
  const filename = fs.realpathSync(path.resolve(directory, profile.buildProfile, 'HomeTiles.ino.bin'));
  const bytes = fs.readFileSync(filename);
  assertReleaseMetadata(parseFirmwareMetadata(bytes), profile);
  return { path: filename, bytes: bytes.length,
    sha256: createHash('sha256').update(bytes).digest('hex') };
}

// Compare application images, not the fixed-size padded factory images. Both
// sides must identify the requested board and exact silicon revision range.
export function compareFirmwareSizes(baseline, candidate, profiles) {
  if (!profiles.length || new Set(profiles).size !== profiles.length) {
    throw new Error('Select one or more distinct build profiles.');
  }
  const results = profiles.map(name => {
    const profile = getBuildProfile(name);
    const before = inspectFirmware(baseline, profile);
    const after = inspectFirmware(candidate, profile);
    if (before.path === after.path) throw new Error('Baseline and candidate must be different files.');
    const deltaBytes = after.bytes - before.bytes;
    return { profile: profile.buildProfile, deviceKey: profile.metadataDeviceKey,
      baseline: before, candidate: after, deltaBytes, passed: deltaBytes <= 0 };
  });
  return { passed: results.every(result => result.passed), results };
}

function main(args) {
  const options = { profiles: [] };
  for (let index = 0; index < args.length; index += 2) {
    const option = args[index];
    const value = args[index + 1];
    if (!['--baseline', '--candidate', '--profile', '--report'].includes(option) ||
        !value || value.startsWith('--')) {
      throw new Error('Usage: node tools/compare-firmware-size.mjs --baseline <directory> --candidate <directory> --profile <build-profile> [--profile <build-profile>] [--report <json-path>]');
    }
    if (option === '--profile') options.profiles.push(value);
    else {
      const key = option.slice(2);
      if (options[key]) throw new Error(`Duplicate option: ${option}`);
      options[key] = value;
    }
  }
  if (!options.baseline || !options.candidate) throw new Error('Baseline and candidate directories are required.');
  const report = compareFirmwareSizes(options.baseline, options.candidate, options.profiles);
  if (options.report) {
    const filename = path.resolve(options.report);
    fs.mkdirSync(path.dirname(filename), { recursive: true });
    fs.writeFileSync(filename, `${JSON.stringify(report, null, 2)}\n`);
  }
  for (const result of report.results) {
    console.log(`${result.passed ? 'PASS' : 'FAIL'} ${result.profile}: ` +
      `${result.baseline.bytes} -> ${result.candidate.bytes} bytes (${result.deltaBytes > 0 ? '+' : ''}${result.deltaBytes})`);
  }
  if (!report.passed) process.exitCode = 1;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  try { main(process.argv.slice(2)); }
  catch (error) { console.error(error.message); process.exitCode = 1; }
}
