// Preserve offsets while masking comments and literals for brace-based extraction.
export function maskCpp(source) {
  return source.replace(/(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'/g,
    (token) => token.replace(/[^\n]/g, ' '));
}

export function cppFunctionDefinitions(source) {
  const masked = maskCpp(source);
  const directives = new Map([...masked.matchAll(/^[ \t]*#(if|ifdef|ifndef|elif|else|endif)\b[^\n]*/gm)]
    .map((match) => [match.index, match[1]]));
  const definitions = [];
  const signature = /^(?:(?:static|inline)\s+)*(?:[\w:<>&*]+\s+)+([\w:]+)\([^;{}]*\)\s*\{/gm;
  for (const match of masked.matchAll(signature)) {
    const bodyStart = match.index + match[0].lastIndexOf('{');
    let depth = 1;
    let end = bodyStart + 1;
    const branches = [];
    while (depth && end < masked.length) {
      const directive = directives.get(end);
      if (directive === 'if' || directive === 'ifdef' || directive === 'ifndef') {
        branches.push(depth);
      } else if (directive === 'else' || directive === 'elif') {
        // Alternative preprocessor branches may each open the same scope.
        depth = branches.at(-1);
      } else if (directive === 'endif') {
        branches.pop();
      }
      if (masked[end] === '{') depth++;
      if (masked[end] === '}') depth--;
      end++;
    }
    if (depth) throw new Error(`Unclosed C++ function: ${match[1]}`);
    definitions.push({name: match[1], start: match.index, end,
      source: source.slice(match.index, end), body: source.slice(bodyStart, end)});
  }
  return definitions;
}

// Compare code tokens while allowing comment translation and whitespace changes.
export function cppTokens(source) {
  return source.match(/(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|\w+|[^\s]/g)
    ?.filter((token) => !token.startsWith('//') && !token.startsWith('/*')) ?? [];
}
