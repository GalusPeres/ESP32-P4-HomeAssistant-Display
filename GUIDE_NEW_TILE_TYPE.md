# New Tile Type (Short Guide)

This project uses a type registry. To add a new tile type, you only touch the
type folder and the registry.

## 1) Add enum value
Edit `src/tiles/tile_config.h` and add a new `TileType` value (next free id).

## 2) Create a type folder
Create `src/types/<name>/` with these files:
- `renderer.h/.cpp` -> `render_<name>_tile(...)`
- `web_html.h/.cpp` -> `append_<name>_fields_html(...)`
- `web_styles.h/.cpp` -> `append_<name>_styles(...)`
- `web_scripts.h/.cpp` -> `append_<name>_scripts(...)`
  - must expose: `load<Name>Fields(tab, data)`, `save<Name>Fields(tab, formData)`,
    `reset<Name>Fields(tab)`
- `web_handler.h/.cpp` -> `apply_<name>_fields_from_request(...)`

Use an existing type (e.g. `sensor`, `scene`, `image`) as a template.

## 3) Register the type
Edit `src/types/types_registry.cpp`:
- include the new headers
- add a new entry in `kTileTypes`

Example entry:
```
{
  TILE_MYTYPE,
  "My Type",
  "mytype",
  "mytype",
  "none",
  nullptr,
  "loadMyTypeFields",
  "saveMyTypeFields",
  "resetMyTypeFields",
  0x353535,
  false,
  render_mytype_wrapper,
  apply_mytype_wrapper,
  append_mytype_fields_wrapper,
  append_mytype_styles,
  append_mytype_scripts
},
```

Notes:
- `fields_suffix` must match the HTML container id: `<tab>_<fields>_fields`.
- `preview_kind` can be `sensor`, `switch`, `clock`, or `none`.
- `save<Name>Fields` drives save + draft + clipboard (same fields everywhere).

## 4) Store data in Tile fields
Use existing `Tile` fields (e.g. `sensor_entity`, `scene_alias`, `key_macro`,
`sensor_decimals`) for your custom data to avoid storage migration.

## 5) Build and test
Compile, add the new type in the Web-Admin, save, reload, and verify the tile
renders on the device.
