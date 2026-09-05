// The Strings and LocaleProfile tables in src/core/i18n/i18n.cpp use positional
// initializers. A field added to the struct without updating every language
// table shifts all following texts silently, and a short fixed-size array
// leaves null entries behind that only crash at runtime. This test compares the
// declared layout against every language table.
import assert from 'node:assert/strict';

import {readRepoFile} from '../../lib/admin-source.mjs';

const header = readRepoFile("src/core/i18n/i18n.h");
const source = readRepoFile("src/core/i18n/i18n.cpp");

// Reads the members of one struct as {name, arraySize} in declaration order.
function structFields(name) {
  const match = new RegExp(`struct ${name} \\{([\\s\\S]*?)\\n\\};`).exec(header);
  assert.ok(match, `struct ${name} must exist in i18n.h`);
  const fields = [];
  const member = /^[ \t]*(?:const char\*|uint8_t|int8_t|uint16_t|bool)[ \t]+(\w+)(?:\[(\d+)\])?[ \t]*;/gm;
  for (const field of match[1].matchAll(member)) {
    fields.push({name: field[1], arraySize: field[2] ? Number(field[2]) : 0});
  }
  assert.ok(fields.length, `struct ${name} must declare members`);
  return fields;
}

// Splits one brace-delimited initializer into its top-level entries. Strings,
// escapes and comments are skipped so their braces and commas do not count.
function initializerEntries(text, openIndex) {
  assert.equal(text[openIndex], '{', 'initializer must start at a brace');
  const entries = [];
  let depth = 0;
  let start = openIndex + 1;
  for (let i = openIndex; i < text.length; i++) {
    const char = text[i];
    const next = text[i + 1];
    if (char === '/' && next === '/') {
      i = text.indexOf('\n', i);
      if (i < 0) break;
      continue;
    }
    if (char === '/' && next === '*') {
      i = text.indexOf('*/', i) + 1;
      continue;
    }
    if (char === '"' || char === "'") {
      for (i++; i < text.length; i++) {
        if (text[i] === '\\') i++;
        else if (text[i] === char) break;
      }
      continue;
    }
    if (char === '{') {
      depth++;
      if (depth === 1) start = i + 1;
      continue;
    }
    if (char === '}') {
      depth--;
      if (depth === 0) {
        entries.push({text: text.slice(start, i), end: i});
        break;
      }
      continue;
    }
    if (char === ',' && depth === 1) {
      entries.push({text: text.slice(start, i), end: i});
      start = i + 1;
    }
  }
  assert.equal(depth, 0, 'initializer braces must be balanced');
  // A trailing comma leaves an empty last entry that carries no value.
  const last = entries[entries.length - 1];
  if (last && !/[^\s]/.test(last.text.replace(/\/\/[^\n]*/g, ''))) entries.pop();
  return entries;
}

function tableNames(structName) {
  const found = [...source.matchAll(
    new RegExp(`static const ${structName} (\\w+) =`, 'g'))].map(m => m[1]);
  assert.ok(found.length >= 3,
    `i18n.cpp must define at least the three ${structName} tables`);
  return found;
}

for (const structName of ['Strings', 'LocaleProfile']) {
  const fields = structFields(structName);
  const tables = tableNames(structName);
  for (const table of tables) {
    const declaration = source.indexOf(`static const ${structName} ${table} =`);
    const entries = initializerEntries(source, source.indexOf('{', declaration));
    assert.equal(entries.length, fields.length,
      `${table} initializes ${entries.length} of the ${fields.length} ` +
      `${structName} members; a positional table must fill every member`);
    fields.forEach((field, index) => {
      if (!field.arraySize) return;
      const nested = entries[index].text;
      const brace = nested.indexOf('{');
      assert.notEqual(brace, -1,
        `${table}.${field.name} must be a braced list of ${field.arraySize} entries`);
      const items = initializerEntries(nested, brace);
      assert.equal(items.length, field.arraySize,
        `${table}.${field.name} has ${items.length} entries but is declared ` +
        `with ${field.arraySize}`);
    });
  }
  console.log(
    `${structName}: ${fields.length} members, ` +
    `${tables.length} tables aligned (${tables.join(', ')})`);
}

// Every declared language must be registered with a matching pair of tables,
// otherwise a locale is unreachable or falls back to another language.
const registry = /static const LanguageEntry kLanguages\[\] = \{([\s\S]*?)\n\};/
  .exec(source);
assert.ok(registry, 'kLanguages must exist in i18n.cpp');
const registered = [...registry[1].matchAll(/\{&(\w+),\s*&(\w+)\}/g)];
const stringTables = new Set(tableNames('Strings'));
const localeTables = new Set(tableNames('LocaleProfile'));
assert.equal(registered.length, stringTables.size,
  'every Strings table must be registered in kLanguages exactly once');
const seen = new Set();
for (const [, strings, locale] of registered) {
  assert.ok(stringTables.has(strings), `kLanguages references unknown ${strings}`);
  assert.ok(localeTables.has(locale), `kLanguages references unknown ${locale}`);
  assert.ok(!seen.has(strings), `${strings} is registered more than once`);
  seen.add(strings);
  assert.equal(strings.replace('kStrings', ''), locale.replace('kLocale', ''),
    `${strings} must be paired with the locale profile of the same language`);
}

console.log(`kLanguages registers ${registered.length} complete languages.`);
