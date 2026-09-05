import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
// A directory named "build" also owns host tests. Broad artifact ignore rules
// previously hid all of them from commits while local discovery still passed.
const sourcePaths = ['tools/tests/build/test-example.mjs', 'tools/tests/core/test-example.mjs'];
for (const source of sourcePaths) {
  const ignored = spawnSync('git', ['check-ignore', '--no-index', source], {cwd: root, encoding: 'utf8'});
  assert.equal(ignored.status, 1, `Test source is ignored: ${source}\n${ignored.stdout}${ignored.stderr}`);
}
const artifact = spawnSync('git', ['check-ignore', '--no-index', 'build/tests/example/test.cpp'],
  {cwd: root, encoding: 'utf8'});
assert.equal(artifact.status, 0, 'Generated test artifacts must remain ignored');

const buildRoot = path.join(root, 'build', 'tests');
fs.mkdirSync(buildRoot, {recursive: true});
const fixture = fs.mkdtempSync(path.join(buildRoot, 'test-discovery-'));
try {
  const write = (name, text) => {
    const filename = path.join(fixture, name);
    fs.mkdirSync(path.dirname(filename), {recursive: true});
    fs.writeFileSync(filename, text);
  };
  // Execute the production runner against isolated nested harnesses.
  write('run-tests.mjs', fs.readFileSync(path.join(root, 'tools/run-tests.mjs'), 'utf8'));
  write('tests/build/test-build.mjs', 'console.log("build passed");');
  write('tests/core/nested/test-core.mjs', 'console.log("core passed");');
  write('tests/web/ignored.mjs', 'throw new Error("Not a test");');
  const run = (...filters) => spawnSync(process.execPath,
    [path.join(fixture, 'run-tests.mjs'), ...filters], {encoding: 'utf8'});
  let result = run();
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /2 passed, 0 skipped, 0 failed/);
  assert.match(result.stdout, /core\/nested\/test-core.mjs/);
  result = run('build/');
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /1 passed, 0 skipped, 0 failed/);
  assert.equal(run('missing-topic').status, 1);
  write('tests/core/nested/test-core.mjs', 'console.log("SKIP: missing optional tool");');
  result = run();
  assert.equal(result.status, 0);
  assert.match(result.stdout, /1 passed, 1 skipped, 0 failed/);
  write('tests/core/nested/test-core.mjs', 'process.exit(1);');
  result = run();
  assert.equal(result.status, 1);
  assert.match(result.stdout, /1 passed, 0 skipped, 1 failed/);
} finally {
  assert.equal(path.dirname(path.resolve(fixture)), path.resolve(buildRoot));
  fs.rmSync(fixture, {recursive: true, force: true});
}
console.log('Recursive test discovery, result reporting and source tracking passed.');
