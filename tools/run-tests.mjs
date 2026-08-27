// Runs every tools/test-*.mjs harness. The workflow used to list the tests by
// hand, which silently left new harnesses out of CI; discovering them here keeps
// the suite and the pipeline in sync.
//
// Usage: node tools/run-tests.mjs [name-fragment ...]
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const toolsDir = path.dirname(fileURLToPath(import.meta.url));
const filters = process.argv.slice(2);

const tests = fs.readdirSync(toolsDir)
  .filter(name => name.startsWith('test-') && name.endsWith('.mjs'))
  .filter(name => !filters.length || filters.some(f => name.includes(f)))
  .sort();

if (!tests.length) {
  console.error(filters.length
    ? `No test matches: ${filters.join(', ')}`
    : 'No tests found in tools/');
  process.exit(1);
}

const failed = [];
const skipped = [];
const started = Date.now();

for (const test of tests) {
  const run = spawnSync(process.execPath, [path.join(toolsDir, test)],
    {encoding: 'utf8'});
  const output = `${run.stdout || ''}${run.stderr || ''}`;
  if (run.status === 0) {
    // A harness that cannot find its optional tooling reports a skip instead of
    // failing, so a missing browser does not look like a firmware defect.
    const isSkip = /^SKIP:/m.test(output);
    if (isSkip) skipped.push(test);
    console.log(`${isSkip ? 'skip' : 'pass'}  ${test}`);
    continue;
  }
  failed.push(test);
  console.log(`FAIL  ${test}`);
  console.log(output.trimEnd().split('\n').map(line => `      ${line}`).join('\n'));
}

const seconds = ((Date.now() - started) / 1000).toFixed(1);
console.log(
  `\n${tests.length - failed.length - skipped.length} passed, ` +
  `${skipped.length} skipped, ${failed.length} failed in ${seconds}s`);
if (skipped.length) console.log(`skipped: ${skipped.join(', ')}`);
if (failed.length) {
  console.log(`failed: ${failed.join(', ')}`);
  process.exit(1);
}
