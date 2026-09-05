import fs from 'node:fs';
import path from 'node:path';
import { Script } from 'node:vm';

const manifestPath = 'src/web/admin/bundle.json';

function findSourceFiles(repoRoot) {
  const files = [];
  function visit(relative) {
    for (const entry of fs.readdirSync(path.join(repoRoot, relative), { withFileTypes: true })) {
      const child = `${relative}/${entry.name}`;
      if (entry.isDirectory()) visit(child);
      else if (entry.isFile() && entry.name.endsWith('.js') &&
          (child.startsWith('src/web/admin/') || /^admin[^/]*\.js$/.test(entry.name))) {
        files.push(child);
      }
    }
  }
  visit('src/web/admin');
  visit('src/types');
  return files;
}

// The browser still receives one classic script. Keeping source order and
// bytes intact preserves its shared scope, initialization order and gzip size.
export function readAdminBundle(repoRoot) {
  const manifest = JSON.parse(fs.readFileSync(path.join(repoRoot, manifestPath), 'utf8'));
  if (manifest.version !== 1 || !Array.isArray(manifest.sources) || !manifest.sources.length) {
    throw new Error(`Invalid Admin bundle manifest: ${manifestPath}`);
  }
  const seen = new Set();
  const sources = manifest.sources.map(relative => {
    if (typeof relative !== 'string' || relative.includes('..') ||
        !/^src\/(?:web\/admin\/[^\\]+|types\/[^/\\]+\/admin[^/\\]*)\.js$/.test(relative)) {
      throw new Error(`Invalid Admin source path: ${relative}`);
    }
    if (seen.has(relative)) throw new Error(`Duplicate Admin source: ${relative}`);
    seen.add(relative);
    const source = fs.readFileSync(path.join(repoRoot, relative), 'utf8').replace(/\r\n?/g, '\n');
    if (!source.endsWith('\n')) throw new Error(`Admin source needs a final newline: ${relative}`);
    // Parse each unit without executing browser code. A declaration or event
    // handler must be complete within its owning source file.
    new Script(source, { filename: relative });
    return source;
  });
  const bundle = sources.join('');
  new Script(bundle, { filename: 'src/web/assets/admin.js' });
  for (const source of findSourceFiles(repoRoot)) {
    if (!seen.has(source)) throw new Error(`Admin source is missing from the manifest: ${source}`);
  }
  return { source: bundle, files: [...seen] };
}
