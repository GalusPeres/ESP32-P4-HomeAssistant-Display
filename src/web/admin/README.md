# Web Admin source

This directory owns the shared browser editor. Type-specific browser code
lives beside its firmware module in `src/types/<type>/admin*.js`.

`bundle.json` lists complete source units in execution order. The asset
generator assembles them into `src/web/assets/admin.js`, then creates the
compressed includes and content hashes used by firmware. Edit source units,
not the assembled file.

```text
node tools/generate-web-assets.mjs
node tools/test-admin-bundle.mjs
node tools/run-tests.mjs
```

The browser still loads one classic script. These source units retain the
existing shared scope and function names; they are not independently loaded
ES modules. Assembly adds no wrappers, requests, runtime lookups or bytes.
The extraction preserved the original normalized JavaScript and gzip bytes.

| Directory | Responsibility |
| --- | --- |
| `core` | Localization helpers and DOM-ready bootstrap |
| `settings` | Display preferences, network fields, access controls and diagnostics actions |
| `navigation` | Top-level tab activation and responsive panel sizing |
| `folders` | Lazy folder fragments, selection and bounded session cache |
| `files` | File-manager state, selection and chunked uploads |
| `firmware` | Browser OTA upload and GitHub update controls |
| `hardware` | Local hardware I/O editor |
| `screensaver` | Screensaver layout, wallpaper and clock editing |
| `tiles/state.js` | Shared tile/editor request and entity-cache state |
| `tiles/registry.js` | Type metadata, accessibility and callback dispatch |
| `tiles/snapshots.js`, `drafts.js`, `clipboard.js` | Saved-field snapshots, local drafts and copy/paste |
| `tiles/editor.js`, `type-selection.js` | Selection, rebinding and type changes |
| `tiles/live-preview.js`, `grid-preview.js` | Selected editor and cached-grid rendering |
| `tiles/autosave.js`, `import-export.js` | Persistence requests, serialization and import |
| `tiles/layout.js`, `drag-geometry.js`, `placement.js`, `drag-resize.js` | Grid geometry and interactions |

The generator parses each source unit and the complete bundle without
executing browser code. Every source file is listed once; an unlisted file
fails validation. DOM/browser tests continue to execute the generated
production functions, and `test-admin-bundle.mjs` verifies that compressed
firmware content matches the assembled source.

When adding a type, use its existing registry load/save/reset callbacks and
central translations. New type code must still support both preview paths,
drafts, autosave and import/export. See [CONTRIBUTING.md](../../../CONTRIBUTING.md)
and [ARCHITECTURE.md](../../../ARCHITECTURE.md) for the full integration path
and the remaining shared-state boundaries.
