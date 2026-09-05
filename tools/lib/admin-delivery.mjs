import {isDeepStrictEqual} from 'node:util';
import {parse} from 'acorn';
import {minify} from 'terser';

function isOrdinaryShorthandProperty(node, parent) {
  return parent?.type === 'ObjectExpression' && node.type === 'Property' &&
    node.kind === 'init' && node.computed === false && node.method === false &&
    node.key.type === 'Identifier' && node.value.type === 'Identifier' &&
    node.key.name === node.value.name && node.key.name !== '__proto__';
}

function comparableAst(value, parent = null) {
  if (Array.isArray(value)) return value.map(item => comparableAst(item, parent));
  if (!value || typeof value !== 'object' || value instanceof RegExp) return value;
  const result = {};
  for (const [key, child] of Object.entries(value)) {
    // Literal.raw is a lexical spelling. TemplateElement.value.raw is observable
    // by tagged templates and must remain part of the comparison.
    if (value.type && ['start', 'end', 'loc', 'raw'].includes(key)) continue;
    result[key] = comparableAst(child, value);
  }
  // The printer may choose {name} or {name: name}. Only ordinary matching
  // identifier properties qualify; prototype setters and patterns do not.
  if (isOrdinaryShorthandProperty(value, parent)) result.shorthand = false;
  return result;
}

export function assertAdminDeliveryEquivalent(source, delivery) {
  const options = {ecmaVersion: 'latest', sourceType: 'script'};
  const sourceAst = comparableAst(parse(source, options));
  const deliveryAst = comparableAst(parse(delivery, options));
  if (!isDeepStrictEqual(sourceAst, deliveryAst)) {
    throw new Error('Admin delivery AST changed beyond lexical formatting or ordinary data-property shorthand.');
  }
}

// This runs only on the host. Keep readable source units and the assembled
// admin.js intact; the firmware stores only the formatted delivery gzip.
export async function formatAdminDelivery(source) {
  const result = await minify(source, {
    compress: false,
    mangle: false,
    module: false,
    format: {comments: 'some', ecma: 2020},
  });
  if (typeof result.code !== 'string') throw new Error('Admin delivery formatter produced no JavaScript.');
  assertAdminDeliveryEquivalent(source, result.code);
  return result.code;
}
