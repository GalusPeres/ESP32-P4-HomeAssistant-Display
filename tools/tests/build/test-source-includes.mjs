import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const sourceExtension = /\.(?:ino|c|cc|cpp|cxx|h|hh|hpp|hxx|inc|ipp)$/i;

function projectFiles(root) {
  const files = [];
  function visit(directory, prefix, recursive) {
    for (const entry of fs.readdirSync(directory, {withFileTypes: true})) {
      const relative = prefix + entry.name;
      if (entry.isFile()) files.push(relative);
      else if (recursive && entry.isDirectory()) {
        visit(path.join(directory, entry.name), relative + '/', true);
      }
    }
  }
  visit(root, '', false);
  visit(path.join(root, 'src'), 'src/', true);
  return files.sort();
}

function quotedIncludes(source) {
  // Consume comments and literals as whole tokens, so examples inside them do
  // not become dependencies. Capture include directives before their quotes.
  const tokens = /\/\*[\s\S]*?\*\/|\/\/[^\n]*|R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|^[ \t]*#[ \t]*include[ \t]*"([^"\r\n]+)"/gm;
  return [...source.matchAll(tokens)].filter(match => match[2]).map(match => ({
    include: match[2],
    line: source.slice(0, match.index).split('\n').length,
  }));
}

function validateIncludes(root) {
  const files = projectFiles(root);
  const exactFiles = new Set(files);
  const foldedFiles = new Map(files.map(file => [file.toLowerCase(), file]));
  const projectBasenames = new Set(files.map(file => path.posix.basename(file).toLowerCase()));
  const errors = [];
  let checked = 0;
  for (const file of files.filter(file => sourceExtension.test(file))) {
    const source = fs.readFileSync(path.join(root, file), 'utf8');
    for (const {include, line} of quotedIncludes(source)) {
      const candidates = [
        path.posix.normalize(path.posix.join(path.posix.dirname(file), include)),
        path.posix.normalize(include),
      ];
      if (candidates.some(candidate => exactFiles.has(candidate))) {
        checked++;
        continue;
      }
      const isBasename = !/[\\/]/.test(include);
      const isProject = /^src[\\/]/i.test(include) || /^\.\.?[\\/]/.test(include) ||
        (isBasename && projectBasenames.has(include.toLowerCase()));
      // Arduino and library headers can be quoted too. Unknown bare names and
      // external prefixes such as freertos/ are resolved by the toolchain.
      if (!isProject) continue;
      checked++;
      const actual = candidates.map(candidate => foldedFiles.get(candidate.toLowerCase())).find(Boolean);
      errors.push({file, line, include, kind: actual ? 'case' : 'missing', actual});
    }
  }
  return {errors, checked};
}

// These fixtures exercise failure detection on case-insensitive hosts too.
// Keep them below the normal build directory and remove only this test's root.
const buildRoot = path.join(repoRoot, 'build', 'tests');
fs.mkdirSync(buildRoot, {recursive: true});
const fixtureRoot = fs.mkdtempSync(path.join(buildRoot, 'source-includes-'));
try {
  const write = (relative, source) => {
    const filename = path.join(fixtureRoot, relative);
    fs.mkdirSync(path.dirname(filename), {recursive: true});
    fs.writeFileSync(filename, source);
  };
  write('App.ino', '#include "src/new/nested/state.h"\n');
  write('RootConfig.h', '#pragma once\n');
  write('src/new/nested/state.h', '#pragma once\n');
  const module = 'src/new/nested/module.cpp';
  write(module, [
    '#include "state.h"',
    '#include "RootConfig.h"',
    '#include "Arduino.h"',
    '#include "freertos/FreeRTOS.h"',
    '// #include "src/comment-missing.h"',
    '/*\n#include "src/block-comment-missing.h"\n*/',
    'const char* example = R"example(\n#include "src/string-missing.h"\n)example";',
  ].join('\n'));
  assert.deepEqual(validateIncludes(fixtureRoot).errors, [],
    'Nested local/project headers and external headers must remain valid');

  for (const [include, kind] of [
    ['src/new/nested/missing.h', 'missing'],
    ['src/New/nested/state.h', 'case'],
    ['State.h', 'case'],
    ['rootconfig.h', 'case'],
  ]) {
    write(module, `#include "${include}"\n`);
    const errors = validateIncludes(fixtureRoot).errors;
    assert.equal(errors.length, 1, `Expected one error for ${include}`);
    assert.equal(errors[0].include, include);
    assert.equal(errors[0].kind, kind);
  }
  write(module, '#include "state.h"\n');
  write('src/consumer.cpp', '#include "state.h"\n');
  assert.deepEqual(validateIncludes(fixtureRoot).errors.map(({file, kind}) => ({file, kind})),
    [{file: 'src/consumer.cpp', kind: 'missing'}],
    'A project basename does not resolve merely because it exists in another folder');
} finally {
  assert.equal(path.dirname(path.resolve(fixtureRoot)), path.resolve(buildRoot));
  fs.rmSync(fixtureRoot, {recursive: true, force: true});
}

const result = validateIncludes(repoRoot);
assert.deepEqual(result.errors, [], result.errors.map(error =>
  `${error.file}:${error.line}: ${error.kind === 'case' ? 'Header case mismatch' : 'Missing project header'} ` +
  `"${error.include}"${error.actual ? ` (file is ${error.actual})` : ''}`).join('\n'));
console.log(`Source includes passed: ${result.checked} project references and negative self-checks.`);
