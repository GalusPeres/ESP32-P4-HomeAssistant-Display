import assert from "node:assert/strict";
import fs from "node:fs";

const read = (path) => fs.readFileSync(path, "utf8");

const configHeader = read("src/core/config_manager.h");
const configSource = read("src/core/config_manager.cpp");
const tileConfig = read("src/tiles/tile_config.cpp");
const adminHtml = read("src/web/web_admin_html.cpp");
const adminJs = read("src/web/assets/admin.js");
const adminRoutes = read("src/web/web_admin.cpp");
const adminHandlers = read("src/web/web_admin_handlers.cpp");

assert.match(configHeader, /bool settings_tile_visible;/);
assert.match(configHeader, /saveSettingsTileVisible\(bool visible\)/);
assert.match(configSource, /getBool\("settings_tile", true\)/);
assert.match(configSource, /putBool\("settings_tile", visible\)/);
assert.match(
  tileConfig,
  /if \(!configManager\.getConfig\(\)\.settings_tile_visible\)[\s\S]*?grid\.tiles\[i\] = Tile\(\)/,
);
assert.match(adminHtml, /saveSettingsTileVisibility\(this\.checked\)/);
assert.match(adminJs, /fetch\('\/api\/display\/settings-tile'/);
assert.match(adminRoutes, /"\/api\/display\/settings-tile"/);
assert.match(adminHandlers, /tileConfig\.saveFolderGrid\(0, grid\)/);
assert.match(adminHandlers, /tiles_request_reload_if_loaded\(GridType::TAB0\)/);

console.log("Settings tile visibility contract: PASS");
