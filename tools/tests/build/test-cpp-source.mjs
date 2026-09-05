import assert from 'node:assert/strict';
import {cppFunctionDefinitions, cppTokens, maskCpp} from '../../lib/cpp-source.mjs';

// Embedded HTML has quotes, comment markers and braces that are literal data.
const raw = 'u8R"html(<div title="/* visible */"> // text\n{ "quoted" }</div>)html"';
const source = `const char* page() {\n  // Description\n  return ${raw};\n}\nvoid next() { work(); }`;
const renamedComment = source.replace('Description', 'Translated description');
assert.deepEqual(cppTokens(source), cppTokens(renamedComment));
assert.ok(cppTokens(source).includes(raw));
assert.notDeepEqual(cppTokens(source), cppTokens(source.replace('visible', 'changed')));
assert.equal(maskCpp(source).length, source.length);
assert.equal(maskCpp(source).split('\n').length, source.split('\n').length);
const definitions = cppFunctionDefinitions(source);
assert.deepEqual(definitions.map(definition => definition.name), ['page', 'next']);
assert.equal(definitions[0].body, `{\n  // Description\n  return ${raw};\n}`);
for (const prefix of ['', 'u8', 'u', 'U', 'L']) {
  for (const delimiter of ['', 'html', 'abcdefghijklmnop']) {
    const literal = `${prefix}R"${delimiter}(/* kept */ // kept\n"quoted" {})${delimiter}"`;
    assert.deepEqual(cppTokens(literal + ' // ignored'), [literal]);
  }
}
console.log('C++ extraction preserves raw strings and ignores only real comments.');
