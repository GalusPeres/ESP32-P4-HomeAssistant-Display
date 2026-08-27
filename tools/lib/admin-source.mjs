// Shared access to the Admin sources for the browser contract tests. The DOM
// harnesses must run the real production functions, so every test cuts them
// out of admin.js instead of re-implementing them.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

export const repoRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)), '..', '..');

export function readRepoFile(...segments) {
  return fs.readFileSync(path.join(repoRoot, ...segments), 'utf8');
}

export const adminSource = readRepoFile('src', 'web', 'assets', 'admin.js');

// Returns the complete declaration of a top-level admin.js function, including
// an `async` prefix, so the harness executes the shipped code path.
export function extractFunction(name, source = adminSource) {
  const marker = `function ${name}(`;
  const start = source.indexOf(marker);
  assert.notEqual(start, -1, `${name} must exist in admin.js`);
  const declarationStart =
    source.slice(Math.max(0, start - 6), start) === 'async ' ? start - 6 : start;
  const bodyStart = source.indexOf('{', start + marker.length);
  assert.notEqual(bodyStart, -1, `${name} must have a body`);
  let depth = 0;
  for (let index = bodyStart; index < source.length; index++) {
    const char = source[index];
    if (char === '{') depth++;
    if (char !== '}') continue;
    depth--;
    if (depth === 0) return source.slice(declarationStart, index + 1);
  }
  assert.fail(`${name} body is incomplete`);
}

// Inline harnesses are embedded in a <script> block, so a literal closing tag
// inside extracted production code would end that block early.
export function inlineScriptSafe(code) {
  return code.replaceAll('</script', '<\\/script');
}
