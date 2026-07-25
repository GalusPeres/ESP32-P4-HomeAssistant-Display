#include "src/web/web_admin_styles.h"
#include "src/web/web_admin_fonts.h"
#include "src/types/types_registry.h"
#include "src/types/climate/layout.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer_fonts.h"

namespace {

constexpr int kPreviewTargetHeight = 430;
constexpr int kWideFourRowPreviewTargetHeight = 390;
constexpr int kPreviewPad = 12;
constexpr int kPreviewGap = 10;

int preview_pad_px() {
  return kPreviewPad;
}

int preview_gap_px() {
  return kPreviewGap;
}

int preview_target_height_px() {
  // Tab5 (7x4) waere bei gleicher Hoehe deutlich breiter als die 8-Zoll-
  // Vorschau (7x5). Mit 390 px erhalten beide 7-Spalten-Profile dieselbe
  // Vorschau-Breite und lassen rechts Platz fuer ein einheitliches Panel.
  if (GRID_COLS == 7 && GRID_ROWS == 4) return kWideFourRowPreviewTargetHeight;
  return kPreviewTargetHeight;
}

int settings_panel_target_width_px() {
  return 390;
}

int admin_wrapper_target_width_px() {
  // Beim kompakten B4 schliesst der gesamte Desktop-Container direkt um
  // Vorschau, Abstand und Settings-Panel. Die breiten Geraete behalten das
  // bisherige 1200-px-Layout.
  return (GRID_COLS <= 4) ? 952 : 1200;
}

int preview_cell_h_px() {
  const int pad = preview_pad_px();
  const int gap = preview_gap_px();
  int cell = (preview_target_height_px() - (2 * pad) - (gap * (GRID_ROWS - 1))) / GRID_ROWS;
  return (cell < 40) ? 40 : cell;
}

int preview_cell_w_px() {
  const int cell_h = preview_cell_h_px();
  int cell_w = (GRID_CELL_W * cell_h + (GRID_CELL_H / 2)) / GRID_CELL_H;
  return (cell_w < 40) ? 40 : cell_w;
}

int preview_scaled_exact_px(int lvgl_px) {
  const int cell_h = preview_cell_h_px();
  const int scaled = (lvgl_px * cell_h + (GRID_CELL_H / 2)) / GRID_CELL_H;
  return scaled < 1 ? 1 : scaled;
}

}  // namespace

namespace {

// LVGL-Pixel -> Vorschau-Pixel im gleichen Verhaeltnis wie Zellhoehe zu
// Display-Zellhoehe. Dadurch stimmen Schriftgroessen in der Web-Vorschau
// per Konstruktion mit dem Display ueberein.
int preview_scaled_px(int lvgl_px) {
  const int cell_h = preview_cell_h_px();
  int v = (lvgl_px * cell_h + (GRID_CELL_H / 2)) / GRID_CELL_H;
  return (v < 6) ? 6 : v;
}

void appendPreviewScaleVars(String& html) {
  auto emit = [&html](const char* name, int lvgl_px) {
    html += "--";
    html += name;
    html += ":";
    html += String(preview_scaled_px(lvgl_px));
    html += "px;";
  };
  auto emit_exact = [&html](const char* name, int lvgl_px) {
    html += "--";
    html += name;
    html += ":";
    html += String(preview_scaled_exact_px(lvgl_px));
    html += "px;";
  };
  auto emit_device_px = [&html](const char* name, int device_px) {
    html += "--";
    html += name;
    html += ":";
    html += String(device_px);
    html += "px;";
  };
  html += "  <style>:root{";
#if defined(DEVICE_LAYOUT_1024X600)
  // Match the compact layout's real LVGL font substitutions. The preview
  // variables describe the rendered font, not the originally requested size.
  emit("fs16", 16);
  emit("fs20", 16);
  emit("fs24", 20);
  emit("fs28", 24);
  emit("fs32", 28);
  emit("fs40", 32);
  emit("fs48", 40);
  emit("fs56", 48);
  emit("fs64", 56);
  emit("fs72", 56);
  emit("fs80", 64);
  emit("fs96", 80);
  emit("icon-size", 40);
  emit_device_px("screensaver-fs20", 16);
  emit_device_px("screensaver-fs24", 20);
  emit_device_px("screensaver-fs28", 24);
  emit_device_px("screensaver-fs32", 28);
  emit_device_px("screensaver-fs40", 32);
  emit_device_px("screensaver-fs48", 40);
  emit_device_px("screensaver-fs56", 48);
  emit_device_px("screensaver-fs64", 56);
  emit_device_px("screensaver-fs72", 56);
  emit_device_px("screensaver-fs80", 64);
  emit_device_px("screensaver-fs96", 80);
  emit_device_px("screensaver-shadow-2", 2);
  emit_device_px("screensaver-shadow-4", 3);
  emit_device_px("screensaver-shadow-6", 5);
#elif defined(DEVICE_LAYOUT_480X480)
  // Native 2/3 font and icon assets used by the 480x480 target.
  emit("fs16", 12);
  emit("fs20", 14);
  emit("fs24", 16);
  emit("fs28", 20);
  emit("fs32", 20);
  emit("fs40", 28);
  emit("fs48", 32);
  emit("fs56", 40);
  emit("fs64", 40);
  emit("fs72", 48);
  emit("fs80", 56);
  emit("fs96", 64);
  emit("icon-size", 32);
  emit_device_px("screensaver-fs20", 14);
  emit_device_px("screensaver-fs24", 16);
  emit_device_px("screensaver-fs28", 20);
  emit_device_px("screensaver-fs32", 20);
  emit_device_px("screensaver-fs40", 28);
  emit_device_px("screensaver-fs48", 32);
  emit_device_px("screensaver-fs56", 40);
  emit_device_px("screensaver-fs64", 40);
  emit_device_px("screensaver-fs72", 48);
  emit_device_px("screensaver-fs80", 56);
  emit_device_px("screensaver-fs96", 64);
  emit_device_px("screensaver-shadow-2", 1);
  emit_device_px("screensaver-shadow-4", 3);
  emit_device_px("screensaver-shadow-6", 4);
#else
  emit("fs16", 16);
  emit("fs20", 20);
  emit("fs24", 24);
  emit("fs28", 28);
  emit("fs32", 32);
  emit("fs40", 40);
  emit("fs48", 48);
  emit("fs56", 56);
  emit("fs64", 64);
  emit("fs72", 72);
  emit("fs80", 80);
  emit("fs96", 96);
  emit("icon-size", 48);      // FONT_MDI_ICONS = mdi_icons_48
  emit_device_px("screensaver-fs20", 20);
  emit_device_px("screensaver-fs24", 24);
  emit_device_px("screensaver-fs28", 28);
  emit_device_px("screensaver-fs32", 32);
  emit_device_px("screensaver-fs40", 40);
  emit_device_px("screensaver-fs48", 48);
  emit_device_px("screensaver-fs56", 56);
  emit_device_px("screensaver-fs64", 64);
  emit_device_px("screensaver-fs72", 72);
  emit_device_px("screensaver-fs80", 80);
  emit_device_px("screensaver-fs96", 96);
  emit_device_px("screensaver-shadow-2", 2);
  emit_device_px("screensaver-shadow-4", 4);
  emit_device_px("screensaver-shadow-6", 6);
#endif
  emit("tile-pad-v", climate_layout::kCardPaddingVertical);
  emit("tile-pad-h", climate_layout::kCardPaddingHorizontal);
#if defined(DEVICE_LAYOUT_1024X600)
  emit("value-dy", 23);
#elif defined(DEVICE_LAYOUT_480X480)
  emit("value-dy", 19);
#else
  emit("value-dy", 28);
#endif
  emit_exact("tile-radius", tile_layout::scale_480(22));
  // Climate tile geometry uses the exact same LVGL-to-preview scale as the
  // device. Keeping these separate from font variables avoids the 6 px
  // minimum used for readable preview text.
  emit_exact("climate-margin-x", climate_layout::kOuterInset);
  emit_exact("climate-grid-gap", climate_layout::kGap);
  emit_exact("climate-slots-top", climate_layout::kContentTop);
  emit_exact("climate-slots-bottom", climate_layout::kOuterInset);
  emit_exact(
      "climate-control-radius",
      climate_layout::kControlRadius);
  emit_exact("climate-control-side-pad", tile_layout::scale_480(8));
  emit_exact("climate-control-caption-w", tile_layout::scale_480(96));
  emit_exact("climate-control-button-w", tile_layout::scale_480(40));
  emit_exact(
      "climate-control-single-w",
      GRID_CELL_W - climate_layout::kOuterInset * 2);
  emit_exact("climate-control-v-pad-top", tile_layout::scale_480(5));
  emit_exact("climate-control-v-pad-bottom", tile_layout::scale_480(5));
  html += "--settings-panel-width:";
  html += String(settings_panel_target_width_px());
  html += "px;";
  html += "--admin-wrapper-width:";
  html += String(admin_wrapper_target_width_px());
  html += "px;";
  const int image_bleed = preview_scaled_exact_px(4);
  const int image_inset = preview_pad_px() > image_bleed
                              ? preview_pad_px() - image_bleed
                              : 0;
  html += "--screensaver-image-inset:";
  html += String(image_inset);
  html += "px;--screensaver-image-radius:";
  html += String(preview_scaled_exact_px(26));
  html += "px;";
  html += "}</style>\n";
}

}  // namespace

void appendAdminStyles(String& html) {
  const int preview_pad = preview_pad_px();
  const int preview_gap = preview_gap_px();
  const int preview_cell_w = preview_cell_w_px();
  const int preview_cell_h = preview_cell_h_px();
  appendPreviewScaleVars(html);
  appendWebFontFaceStyles(html);
  html += R"html(
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@mdi/font@7.4.47/css/materialdesignicons.min.css">
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 48 48'%3E%3Crect width='48' height='48' rx='10' fill='%2316181c'/%3E%3Crect x='4' y='4' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Crect x='27' y='4' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Crect x='4' y='27' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Cpath d='M33 26h5v6.5h6.5v5H38V44h-5v-6.5h-6.5v-5H33z' fill='%2326a69a'/%3E%3C/svg%3E">
  <style>
    :root {
      --bg:#0a0a0a;
      --card:#1c1c1c;
      --panel:#141414;
      --well:#0e0e0e;
      --line:#2a2a2a;
      --line-strong:#333333;
      --text:#f2f2f2;
      --text-2:#b8b8b8;
      --text-3:#8a8a8a;
      --muted:#666666;
      --accent:#26a69a;
      --accent-hover:#2cb5a8;
      --accent-active:#1f8a80;
      --accent-link:#4db6ac;
      --accent-soft:rgba(38,166,154,0.14);
      --accent-ring:rgba(38,166,154,0.22);
      --danger:#c14444;
      --danger-hover:#a83a3a;
      --go:#2e7d32;
      --go-hover:#43a047;
      --go-active:#1b5e20;
      --ok:#66BB6A;
      --err:#FF6B6B;
      accent-color:#26a69a;
      color-scheme:dark;
    }
    * { scrollbar-width:thin; scrollbar-color:#333333 transparent; }
    ::selection { background:rgba(38,166,154,0.35); color:#ffffff; }
    ::-webkit-scrollbar { width:10px; height:10px; }
    ::-webkit-scrollbar-thumb { background:#333333; border-radius:999px; border:2px solid transparent; background-clip:padding-box; }
    ::-webkit-scrollbar-track { background:transparent; }

    body {
      font-family:'HomeTiles Inter', sans-serif;
      background:var(--bg);
      color:var(--text);
      margin:0;
      padding:0;
      height:100vh;
      overflow:hidden;
    }
    .wrapper {
      max-width:var(--admin-wrapper-width, 1200px);
      height:100vh;
      box-sizing:border-box;
      margin:0 auto;
      padding:24px 20px;
    }
    .card {
      background:var(--card);
      border:1px solid var(--line);
      border-radius:22px;
      box-shadow:0 20px 60px rgba(0,0,0,0.5);
      padding:32px;
      box-sizing:border-box;
      height:100%;
      min-height:0;
      display:flex;
      flex-direction:column;
    }

    /* Header / Brand */
    .brand { display:flex; align-items:center; gap:14px; margin-bottom:24px; }
    .brand svg { flex-shrink:0; }
    .brand h1 { margin:0; font-size:24px; font-weight:700; color:#ffffff; letter-spacing:-0.01em; }
    .brand .device { color:var(--text-3); font-size:13px; margin-top:3px; }
    .brand-links { margin-left:auto; display:flex; align-items:center; gap:8px; flex-wrap:wrap; }
    .brand-link {
      display:inline-flex;
      align-items:center;
      gap:7px;
      padding:8px 14px;
      border:1px solid var(--line-strong);
      border-radius:999px;
      background:transparent;
      color:var(--text-2);
      text-decoration:none;
      font-size:13px;
      font-weight:600;
      transition:border-color 0.15s, color 0.15s;
    }
    .brand-link:hover { color:var(--text); border-color:#4a4a4a; }
    .brand-link .mdi { font-size:17px; line-height:1; }
    h1 { margin:0 0 8px; font-size:28px; color:var(--text); }
    .subtitle { color:var(--text-3); margin-bottom:24px; }

    /* Inline WiFi status (section title row) */
    .section-title-row { display:flex; align-items:baseline; justify-content:space-between; gap:12px; margin:0 0 14px; }
    .section-title-row .section-title { margin:0; }
    .wifi-inline-status { font-size:12px; color:var(--text-3); white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .wifi-inline-dot { display:inline-block; width:8px; height:8px; border-radius:50%; background:var(--ok); margin-right:6px; }
    .wifi-inline-dot.off { background:var(--err); }

    /* Tab Navigation */
    .tab-nav { display:flex; gap:6px; margin-bottom:24px; flex-wrap:wrap; }
    .tab-btn {
      padding:10px 18px;
      border:1px solid transparent;
      border-radius:999px;
      background:transparent;
      color:var(--text-3);
      font-size:15px;
      font-weight:600;
      font-family:inherit;
      cursor:pointer;
      transition:background 0.15s, color 0.15s, border-color 0.15s;
      display:flex;
      flex-direction:row;
      align-items:center;
      justify-content:center;
      gap:8px;
    }
    .tab-btn:hover { color:var(--text); background:#232323; }
    .tab-btn.active { color:#ffffff; background:var(--accent); }
    .tab-btn.active:hover { background:var(--accent-hover); }
    .edit-icon { font-size:12px; margin-left:6px; opacity:0.6; cursor:pointer; transition:opacity 0.2s; }
    .edit-icon:hover { opacity:1; }
    .tab-content { display:none; }
    .tab-content.active { display:block; }
    /* Settings bleibt unter der Tab-Leiste und scrollt dort intern, statt den
       Browser-Viewport mit langen Konfigurationsformularen zu vergroessern. */
    #tab-network.active {
      flex:1 1 auto;
      min-height:0;
      max-height:none;
      display:flex;
      flex-direction:column;
      overflow:hidden;
      overscroll-behavior:contain;
    }
    #admin_settings_form {
      flex:1 1 auto;
      min-height:0;
      overflow-y:auto;
      overscroll-behavior:contain;
      margin:0 -12px 0 0;
      padding-right:12px;
      padding-bottom:16px;
    }

    /* Forms */
    form { display:grid; gap:16px; margin-bottom:32px; }
    label { font-size:13px; font-weight:600; color:var(--text-2); display:block; margin-bottom:6px; }
    input, select, textarea {
      width:100%;
      padding:12px 16px;
      border:1px solid var(--line-strong);
      border-radius:12px;
      font-size:15px;
      font-family:inherit;
      box-sizing:border-box;
      background:var(--well);
      color:var(--text);
      transition:border-color 0.15s, box-shadow 0.15s;
    }
    input::placeholder, textarea::placeholder { color:var(--muted); }
    /* Chrome may offer previously entered IP values, but its default blue
       autofill background must not replace the HomeTiles field styling. */
    input:-webkit-autofill,
    input:-webkit-autofill:hover,
    input:-webkit-autofill:focus,
    input:-webkit-autofill:active {
      -webkit-text-fill-color:var(--text);
      caret-color:var(--text);
      -webkit-box-shadow:0 0 0 1000px var(--well) inset;
      box-shadow:0 0 0 1000px var(--well) inset;
      transition:background-color 9999s ease-out 0s;
    }
    input:not([type="checkbox"]):not([type="radio"]):focus,
    select:focus, textarea:focus {
      outline:none;
      border-color:var(--accent);
      box-shadow:0 0 0 3px var(--accent-ring);
    }
    select {
      max-width:100%;
      appearance:none;
      -webkit-appearance:none;
      background-image:url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" width="12" height="8" viewBox="0 0 12 8"><path d="M1 1.5l5 5 5-5" fill="none" stroke="%23888888" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>');
      background-repeat:no-repeat;
      background-position:right 14px center;
      padding-right:38px;
      cursor:pointer;
    }
    select option, select optgroup { background:#1a1a1a; color:var(--text); }
    @supports (appearance: base-select) {
      select, ::picker(select) { appearance:base-select; }
      select::picker-icon { display:none; }
      ::picker(select) {
        background:#1c1c1c;
        border:1px solid var(--line-strong);
        border-radius:12px;
        box-shadow:0 16px 40px rgba(0,0,0,0.55);
        padding:6px;
        margin-top:4px;
      }
      select option {
        padding:9px 12px;
        border-radius:8px;
        background:transparent;
        color:var(--text);
        font-size:14px;
        cursor:pointer;
      }
      select option:hover, select option:focus { background:#2a2a2a; }
      select option:checked { background:var(--accent-soft); color:#5fd4c7; font-weight:600; }
      select option::checkmark { color:var(--accent); }
      select optgroup { background:transparent; font-size:12px; color:var(--text-3); }
    }
    textarea { resize:vertical; }
    input[type="checkbox"], input[type="radio"] {
      width:16px;
      height:16px;
      padding:0;
      cursor:pointer;
      box-shadow:none;
    }
    input[type="checkbox"]:focus, input[type="radio"]:focus {
      outline:none;
      box-shadow:none;
    }
    input[type="range"] { padding:0; border:none; background:transparent; height:32px; box-shadow:none; cursor:pointer; }
    input[type="range"]:focus { box-shadow:none; }
    input[type="color"] { padding:5px 8px; height:44px; cursor:pointer; }
    input[type="file"] { padding:9px 12px; }

    .password-field { display:flex; gap:8px; align-items:center; }
    .password-field input { flex:1 1 auto; }
    .password-toggle {
      flex:0 0 auto;
      padding:12px 14px;
      border:1px solid var(--line-strong);
      border-radius:12px;
      background:var(--well);
      color:var(--text-2);
      cursor:pointer;
      font-size:13px;
      font-weight:600;
      font-family:inherit;
      transition:border-color 0.15s, color 0.15s;
    }
    .password-toggle:hover { border-color:#4a4a4a; color:var(--text); }

    /* Settings sections */
    .settings-section { background:var(--panel); border:1px solid #232323; border-radius:16px; padding:20px; }
    .settings-grid { display:grid; grid-template-columns:repeat(2, minmax(0, 1fr)); gap:16px; }
    .settings-subgrid { display:grid; grid-template-columns:repeat(2, minmax(0, 1fr)); gap:16px; }
    .settings-full { grid-column:1 / -1; }
    .settings-note { font-size:12px; color:var(--text-3); margin-top:4px; }
    .network-settings-group { display:flex; flex-direction:column; gap:16px; padding-top:2px; }
    .network-settings-heading {
      text-transform:uppercase;
      font-size:12px;
      font-weight:700;
      letter-spacing:.1em;
      color:var(--text-3);
    }
    .settings-actions { display:flex; gap:10px; flex-wrap:wrap; }
    /* Alle Buttons in Settings-Aktionszeilen einheitlich kompakt (gleiches
       Mass wie .file-picker), sonst mischen sich grosse .btn, versetzte
       .btn-secondary (margin-top) und kleine Datei-Buttons im selben Bereich. */
    .settings-actions .btn {
      width:auto;
      margin-top:0;
      padding:10px 14px;
      font-size:14px;
      flex:0 0 auto;
    }
    .ota-github-actions { margin-top:8px; }
    .file-picker { display:flex; gap:10px; align-items:center; flex-wrap:wrap; margin-top:8px; }
    .file-picker-name { color:var(--text-3); font-size:14px; overflow-wrap:anywhere; }
    .btn-inline { width:auto; margin-top:0; }
    .file-picker .btn,
    .file-picker .btn-secondary,
    .file-picker .btn-inline {
      width:auto;
      margin-top:0;
      padding:10px 14px;
      font-size:14px;
      flex:0 0 auto;
    }

    /* File Manager */
    .file-manager { grid-template-columns:1fr; align-items:start; }
    .file-manager-topbar { display:flex; align-items:center; justify-content:space-between; gap:12px; flex-wrap:wrap; }
    .file-manager-storage-state { display:inline-flex; align-items:center; min-height:28px; padding:4px 12px; border-radius:999px; background:#2a2a2a; color:var(--text-2); font-size:13px; font-weight:700; white-space:nowrap; }
    .file-manager-storage-state.ok { background:rgba(46,125,50,0.22); color:var(--ok); }
    .file-manager-storage-state.error { background:rgba(239,68,68,0.18); color:var(--err); }
    .file-manager-storage-state.checking { background:var(--accent-soft); color:var(--accent-link); }
    .file-manager-toolbar-group,
    .file-manager-upload-row { display:flex; align-items:center; gap:8px; flex-wrap:wrap; min-height:38px; }
    .file-manager-upload-row { margin-top:8px; }
    .file-manager .file-manager-toolbar-btn,
    .file-manager .file-manager-selection-btn { display:inline-flex; align-items:center; justify-content:center; width:auto; min-width:0; height:36px; margin:0; padding:0 14px; border-radius:999px; font-size:14px; line-height:1.2; flex:0 0 auto; justify-self:start; }
    .file-manager-upload-row .file-picker-name { flex:1 1 260px; min-width:220px; color:var(--text-3); overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
    .file-manager-status { min-height:18px; margin-top:6px; }
    .file-manager-status.error { color:var(--err); }
    .file-manager-status.success { color:var(--ok); }
    .file-manager-selection-bar { display:flex; align-items:center; justify-content:space-between; gap:10px; margin-top:8px; padding:8px 10px; border:1px solid #232323; border-radius:12px; background:var(--well); }
    .file-manager-selection-info { min-width:0; color:var(--text-2); font-size:13px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
    .file-manager-selection-actions { display:flex; align-items:center; justify-content:flex-end; gap:6px; flex-wrap:wrap; }
    .file-manager .file-manager-selection-btn:disabled,
    .file-manager .file-manager-toolbar-btn:disabled { opacity:.45; cursor:not-allowed; }
    .file-manager-breadcrumb { display:flex; align-items:center; gap:4px; min-height:34px; margin:0 0 6px; padding:6px 8px; border:1px solid #232323; border-radius:12px; background:var(--well); overflow-x:auto; white-space:nowrap; }
    .file-manager-breadcrumb-item { display:inline-flex; align-items:center; gap:5px; padding:3px 6px; border:0; border-radius:6px; background:transparent; color:var(--accent-link); font:inherit; font-size:13px; font-weight:600; cursor:pointer; }
    .file-manager-breadcrumb-item:hover:not(:disabled) { background:#222222; }
    .file-manager-breadcrumb-item:disabled { color:var(--text-2); cursor:default; }
    .file-manager-breadcrumb-separator { color:var(--muted); font-size:13px; }
    .file-manager-table-wrap { width:100%; max-height:520px; overflow:auto; border:1px solid #232323; border-radius:14px; background:var(--well); }
    .file-manager-table { width:100%; border-collapse:collapse; font-size:13px; color:var(--text); table-layout:fixed; }
    .file-manager-table th,
    .file-manager-table td { padding:8px 12px; border-bottom:1px solid #1c1c1c; text-align:left; vertical-align:middle; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .file-manager-table th { position:sticky; top:0; z-index:1; background:#161616; color:var(--text-3); font-size:12px; font-weight:700; text-transform:none; letter-spacing:0; }
    .file-manager-table tbody tr { cursor:pointer; outline:none; }
    .file-manager-table tbody tr:nth-child(odd) { background:var(--well); }
    .file-manager-table tbody tr:nth-child(even) { background:#121212; }
    .file-manager-table tbody tr:hover { background:#1e1e1e; }
    .file-manager-table tbody tr:focus { box-shadow:inset 0 0 0 2px var(--accent); }
    .file-manager-table tbody tr.file-manager-row-selected td { background:rgba(38,166,154,0.20) !important; }
    .file-manager-table tr:last-child td { border-bottom:none; }
    .file-manager-table th:nth-child(1), .file-manager-table td:nth-child(1) { width:auto; }
    .file-manager-table th:nth-child(2), .file-manager-table td:nth-child(2) { width:170px; }
    .file-manager-table th:nth-child(3), .file-manager-table td:nth-child(3) { width:110px; }
    .file-manager-name { display:flex; align-items:center; gap:8px; min-width:0; overflow:hidden; text-overflow:ellipsis; }
    .file-manager-name-icon { flex:0 0 auto; width:18px; color:var(--text-3); font-size:17px; text-align:center; }
    .file-manager-name span,
    .file-manager-name-link { min-width:0; overflow:hidden; text-overflow:ellipsis; }
    .file-manager-name-link { padding:0; border:0; background:transparent; color:var(--accent-link); font:inherit; font-weight:600; text-align:left; cursor:pointer; }
    .file-manager-name-link:hover { text-decoration:underline; }
    .file-manager-folder-name { color:var(--accent-link); font-weight:600; }
    .file-manager-parent-row .file-manager-name-icon { color:var(--accent-link); }
    .file-manager-muted { color:var(--text-3); }
    .file-manager-size-cell { color:#a0a0a0; font-family:Consolas, 'Courier New', monospace; }
    .file-manager-parent-row td { background:#161616 !important; }

    /* OTA */
    .ota-version-value {
      display:inline-block;
      font-family:Consolas, 'Courier New', monospace;
      overflow-wrap:anywhere;
      word-break:break-word;
    }
    .ota-status { min-height:18px; margin-top:8px; }
    .ota-status.error { color:var(--err); }
    .ota-status.success { color:var(--ok); }
    .ota-progress {
      width:100%;
      height:10px;
      margin-top:8px;
      border-radius:999px;
      overflow:hidden;
      background:#242424;
    }
    .ota-progress.active::before {
      content:'';
      display:block;
      width:40%;
      height:100%;
      border-radius:999px;
      background:var(--go-hover);
      animation:ota-progress-slide 1.1s ease-in-out infinite;
    }
    .ota-progress-bar {
      width:0%;
      height:100%;
      border-radius:999px;
      background:var(--go-hover);
      transition:width 0.18s ease;
    }
    @keyframes ota-progress-slide {
      0% { transform:translateX(-100%); }
      100% { transform:translateX(250%); }
    }

    .settings-checkbox { display:flex; align-items:center; gap:10px; margin:0; font-size:14px; color:var(--text); }
    .settings-checkbox input { width:auto; padding:0; margin:0; }
    .is-hidden { display:none !important; }

    /* Buttons */
    .btn {
      padding:12px 18px;
      border:none;
      border-radius:12px;
      background:var(--accent);
      color:#ffffff;
      font-size:15px;
      font-weight:600;
      font-family:inherit;
      cursor:pointer;
      transition:background 0.15s;
    }
    .btn:hover { background:var(--accent-hover); }
    .btn:active { background:var(--accent-active); }
    .btn:focus-visible { outline:none; box-shadow:0 0 0 3px var(--accent-ring); }
    .btn-secondary { background:#2a2a2a; color:#e8e8e8; margin-top:12px; width:100%; }
    .btn-secondary:hover { background:#333333; }
    .btn-secondary:active { background:#3a3a3a; }
    .btn-danger { background:var(--danger); }
    .btn-danger:hover { background:var(--danger-hover); }
    .btn-danger:active { background:#8f2e2e; }
    /* Gruen = positive "Los"-Aktion (Speichern/Update) - gleiche Semantik
       wie im LVGL-Settings-Menue auf dem Geraet. */
    .btn-go { background:var(--go); }
    .btn-go:hover { background:var(--go-hover); }
    .btn-go:active { background:var(--go-active); }

    .admin-hidden-form { display:none; }
    .admin-footer-actions {
      display:flex;
      flex:0 0 auto;
      align-items:center;
      justify-content:flex-end;
      gap:12px;
      flex-wrap:wrap;
      margin-top:0;
      padding:16px 12px 0 0;
      border-top:1px solid var(--line);
      background:var(--card);
    }
    .admin-footer-actions .btn { width:auto; min-width:180px; margin-top:0; padding:11px 16px; font-size:14px; }
    .section-title { margin:32px 0 12px; text-transform:uppercase; font-size:12px; font-weight:700; letter-spacing:.1em; color:var(--text-3); }
    .settings-section .section-title { margin:0 0 14px; }
    /* In the title/status row the row itself owns the bottom spacing. Without
       this override NETWORK received the title margin a second time. */
    .settings-section .section-title-row .section-title { margin:0; }
    .hint { color:var(--text-3); font-size:14px; margin:8px 0 16px; }
    .list-block { background:var(--panel); border:1px solid #232323; border-radius:16px; padding:16px; }
    .list-block strong { display:block; margin:12px 0 6px; color:var(--text); }
    .list { list-style:none; padding-left:18px; margin:0; }
    .list li { padding:4px 0; font-family:monospace; color:var(--text-2); }
    .layout-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:16px; }
    .slot { background:var(--panel); border:1px solid #232323; border-radius:16px; padding:12px; }
    .slot-scene { background:rgba(38,166,154,0.06); }
    .slot-label { font-size:13px; font-weight:600; color:var(--text-3); margin-bottom:8px; }
    .slot select, .slot input { width:100%; box-sizing:border-box; }
    .slot select { padding:10px; font-size:15px; margin-bottom:8px; }
    .slot input { padding:9px; font-size:13px; margin-bottom:6px; }
    .legacy-block { border:1px dashed var(--line-strong); background:var(--panel); border-radius:16px; padding:12px; margin-bottom:16px; }

    /* Tab Settings Above Grid */
    .tab-settings-top {
      background:var(--panel);
      border:1px solid #232323;
      border-radius:16px;
      padding:16px;
      margin-bottom:20px;
      max-width:600px;
    }

    /* Tile Editor */
    .tile-editor { display:flex; align-items:flex-start; gap:24px; }
    /* Die Vorschau bleibt immer in ihrer geraetespezifischen Groesse;
       das Settings-Panel nutzt nur den verbleibenden Desktop-Platz. */
    .tile-editor-main { flex:0 0 auto; min-width:0; display:flex; flex-direction:column; gap:14px; }
    .tile-grid-scroll { max-width:100%; overflow-x:auto; }
    /* width:0 + min-width:100% - der Footer richtet sich nach der Grid-Breite,
       ohne dass sein (langer) Hinweistext die Spalte aufblaeht. */
    .folder-footer { display:flex; align-items:center; justify-content:space-between; gap:12px; flex-wrap:wrap; width:0; min-width:100%; }
    .folder-footer-options { flex:0 0 100%; display:flex; align-items:center; gap:16px; }
    .folder-footer-options .inline-checkbox { margin:0; }
    .folder-footer .hint { margin:0; }
    .btn-delete-folder { padding:8px 16px; font-size:13px; flex:0 0 auto; }
    .tile-actions { display:flex; gap:8px; margin-top:8px; }
    .tile-actions .btn {
      flex:1 1 0;
      min-width:0;
      padding:8px 10px;
      font-size:12px;
      white-space:nowrap;
    }
    .tile-grid {
      display:grid;
)html";
  html += "      grid-template-columns:repeat(" + String(GRID_COLS) + ", " + String(preview_cell_w) + "px);\n";
  html += "      grid-template-rows:repeat(" + String(GRID_ROWS) + ", " + String(preview_cell_h) + "px);\n";
  html += "      gap:" + String(preview_gap) + "px;\n";
  html += "      padding:" + String(preview_pad) + "px;\n";
  html += R"html(
      background:#000000;
      border:1px solid var(--line);
      border-radius:21px;
      width:fit-content;
      height:fit-content;
    }
    .screensaver-tile-grid {
      position:relative;
      overflow:hidden;
      isolation:isolate;
      touch-action:none;
      user-select:none;
    }
    .screensaver-grid-image-frame {
      position:absolute;
      inset:var(--screensaver-image-inset);
      z-index:0;
      overflow:hidden;
      border-radius:var(--screensaver-image-radius);
      pointer-events:auto;
      cursor:pointer;
      user-select:none;
    }
    /* Outline liegt ausserhalb des Bildes und veraendert dessen Layout nicht. */
    .screensaver-tile-grid.selected-background .screensaver-grid-image-frame {
      outline:3px solid #26a69a;
      outline-offset:0;
    }
    /* Hover nur vor der Auswahl. Ein ausgewaehlter Hintergrund behaelt
       ausschliesslich seinen durchgehenden Auswahlrahmen. */
    .screensaver-tile-grid:not(.selected-background) .screensaver-grid-image-frame:hover {
      outline:3px dashed rgba(38,166,154,0.6);
      outline-offset:0;
    }
    .screensaver-grid-image {
      position:absolute;
      inset:0;
      width:100%;
      height:100%;
      object-fit:cover;
      pointer-events:none;
      user-select:none;
    }
    .screensaver-grid-image[hidden] { display:none !important; }
    .screensaver-grid-clock {
      position:absolute;
      z-index:3;
      color:#fff;
      text-align:center;
      line-height:1.1;
      transform:translate(-50%,-50%);
      cursor:grab;
      touch-action:none;
      white-space:nowrap;
    }
    /* Grosser, weicher Schatten mit nur kleinem Versatz. Die Display-Version
       bildet denselben Verlauf mit mehreren sehr schwachen Textkopien nach. */
    .screensaver-grid-clock.clock-shadowed {
      text-shadow:
        var(--screensaver-clock-shadow-4, 2.5px) var(--screensaver-clock-shadow-4, 2.5px) 0 rgba(0,0,0,0.133),
        var(--screensaver-clock-shadow-2, 1.25px) var(--screensaver-clock-shadow-4, 2.5px) 0 rgba(0,0,0,0.055),
        var(--screensaver-clock-shadow-6, 3.75px) var(--screensaver-clock-shadow-4, 2.5px) 0 rgba(0,0,0,0.055),
        var(--screensaver-clock-shadow-4, 2.5px) var(--screensaver-clock-shadow-2, 1.25px) 0 rgba(0,0,0,0.055),
        var(--screensaver-clock-shadow-4, 2.5px) var(--screensaver-clock-shadow-6, 3.75px) 0 rgba(0,0,0,0.055),
        var(--screensaver-clock-shadow-2, 1.25px) var(--screensaver-clock-shadow-2, 1.25px) 0 rgba(0,0,0,0.031),
        var(--screensaver-clock-shadow-6, 3.75px) var(--screensaver-clock-shadow-2, 1.25px) 0 rgba(0,0,0,0.031),
        var(--screensaver-clock-shadow-2, 1.25px) var(--screensaver-clock-shadow-6, 3.75px) 0 rgba(0,0,0,0.031),
        var(--screensaver-clock-shadow-6, 3.75px) var(--screensaver-clock-shadow-6, 3.75px) 0 rgba(0,0,0,0.031);
    }
    .screensaver-grid-clock:hover {
      outline:3px dashed rgba(38,166,154,0.65);
      outline-offset:6px;
      border-radius:8px;
    }
    .screensaver-grid-clock.selected-clock {
      outline:3px solid #26a69a;
      outline-offset:6px;
      border-radius:8px;
    }
    .screensaver-grid-clock.clock-disabled::before {
      content:'--:--';
      display:block;
      padding:5px 9px;
      color:rgba(255,255,255,0.55);
      font-size:12px;
      text-shadow:none;
    }
    .screensaver-grid-clock.dragging { cursor:grabbing; }
    #screensaverClockDate { color:#fff; }
    /* Formsprache der Tile-Resize-Handles, aber kontrastreicher: der Griff
       liegt auf beliebig hellen Fotos statt auf dunklem Grid-Hintergrund. */
    .screensaver-clock-resize-handle {
      position:absolute;
      right:-12px;
      bottom:-12px;
      width:14px;
      height:14px;
      display:block;
      box-sizing:border-box;
      border:1px solid rgba(255,255,255,0.65);
      border-radius:6px;
      background:rgba(255,255,255,0.3);
      box-shadow:0 1px 4px rgba(0,0,0,0.5);
      cursor:nwse-resize;
      opacity:0;
      pointer-events:none;
      transition:opacity 0.15s ease;
    }
    .screensaver-grid-clock.selected-clock .screensaver-clock-resize-handle {
      opacity:0.75;
      pointer-events:auto;
    }
    .screensaver-grid-clock.selected-clock:hover .screensaver-clock-resize-handle,
    .screensaver-grid-clock:hover .screensaver-clock-resize-handle {
      opacity:1;
      pointer-events:auto;
    }
    .screensaver-tile-grid > .tile,
    .screensaver-tile-grid > .tile-drop-placeholder,
    .screensaver-tile-grid > .tile-resize-placeholder {
      z-index:2;
    }
    /* Vorschau des optionalen Kachel-Schattens auf dem Geraet. drop-shadow
       statt box-shadow, damit Auswahl-/Hover-Shadows unangetastet bleiben. */
    .screensaver-tile-grid.tiles-shadowed > .tile:not(.empty) {
      filter:drop-shadow(0 2px 8px rgba(0,0,0,0.65));
    }
    /* Die feine Linie sitzt an der aeusseren Kartenkante. Ein Kind-Overlay
       waere wegen des transparenten 3px-Editorrahmens sichtbar eingerueckt. */
    .tile-grid.tiles-bordered > .tile:not(.empty) {
      outline:1px solid rgba(255,255,255,0.20);
      outline-offset:-1px;
    }

    /* Display-aehnliche Kacheln (50% Skalierung) */
    /* Display: Title=TOP_LEFT, Value=CENTER(-30,18), Unit=RIGHT_MID */
    .tile {
      background:#2A2A2A;
      border-radius:var(--tile-radius, 11px);
      cursor:pointer;
      border:3px solid transparent;
      padding:var(--tile-pad-v, 12px) var(--tile-pad-h, 10px);
      position:relative;
      box-sizing:border-box;
      overflow:hidden;
      background-clip:padding-box;
      clip-path:inset(0 round var(--tile-radius, 11px));
    }
    .tile:hover:not(.active) {
      border:3px dashed rgba(38,166,154,0.6);
      box-shadow:0 0 0 2px rgba(38,166,154,0.12) inset;
      border-radius:var(--tile-radius, 11px);
      background-clip:padding-box;
      clip-path:inset(0 round var(--tile-radius, 11px));
    }
    /* Die Auswahl ist pro Ordner in data-selected gespeichert. Die active-
       Klasse kann bei asynchronen Vorschau-Updates wechseln, die sichtbare
       Markierung darf davon aber nicht abhaengen. */
    .tile.active,
    .tile[data-selected="1"] {
      border:3px solid #26a69a;
      box-shadow:0 0 12px rgba(38,166,154,0.55);
      border-radius:var(--tile-radius, 11px);
      background-clip:padding-box;
      clip-path:inset(0 round var(--tile-radius, 11px));
    }
    .tile.dragging {
      opacity:0.02;
      border:3px solid transparent;
      box-shadow:none;
      border-radius:var(--tile-radius, 11px);
      background-clip:padding-box;
      clip-path:inset(0 round var(--tile-radius, 11px));
    }
    .tile.resizing {
      z-index:24;
      opacity:.20;
      box-shadow:0 0 0 2px rgba(38,166,154,0.25) inset, 0 0 18px rgba(38,166,154,0.18);
    }
    .tile.resize-invalid {
      box-shadow:0 0 0 2px rgba(239,68,68,0.22) inset, 0 0 18px rgba(239,68,68,0.2);
    }
    .tile.reflow-preview {
      border:3px solid rgba(100,216,203,0.55);
      box-shadow:0 0 0 2px rgba(100,216,203,0.35) inset, 0 0 18px rgba(38,166,154,0.18);
      filter:brightness(1.06);
    }
    .tile.drop-target {
       border:3px dashed #26a69a;
       background:rgba(38,166,154,0.12);
       box-shadow:0 0 0 2px rgba(38,166,154,0.2) inset;
       border-radius:var(--tile-radius, 11px);
       background-clip:padding-box;
       clip-path:inset(0 round var(--tile-radius, 11px));
    }
    .tile.empty.drop-target {
       border:3px dashed #26a69a;
       background:rgba(38,166,154,0.08);
    }
    .tile-drop-placeholder {
      display:none;
      border:3px dashed #26a69a;
      background:rgba(38,166,154,0.18);
      box-shadow:0 0 0 2px rgba(38,166,154,0.2) inset;
      border-radius:var(--tile-radius, 11px);
      box-sizing:border-box;
      pointer-events:none;
      z-index:20;
      clip-path:inset(0 round var(--tile-radius, 11px));
    }
    .tile-drop-placeholder.show { display:block; }
    .tile-drop-placeholder.invalid {
      border-color:#ef4444;
      background:rgba(239,68,68,0.16);
      box-shadow:0 0 0 2px rgba(239,68,68,0.18) inset;
    }
    .tile-resize-placeholder {
      display:none;
      position:relative;
      overflow:hidden;
      padding:0;
      border:0;
      background:transparent;
      box-shadow:none;
      border-radius:var(--tile-radius, 11px);
      box-sizing:border-box;
      pointer-events:none;
      z-index:26;
      clip-path:inset(0 round var(--tile-radius, 11px));
    }
    .tile-resize-placeholder::after {
      content:"";
      position:absolute;
      z-index:3;
      inset:0;
      box-sizing:border-box;
      border:3px dashed #26a69a;
      border-radius:var(--tile-radius, 11px);
      background:rgba(38,166,154,0.08);
      box-shadow:0 0 0 2px rgba(38,166,154,0.16) inset;
      pointer-events:none;
    }
    .tile-resize-preview-card {
      position:absolute !important;
      z-index:1;
      inset:0;
      width:100%;
      height:100%;
      opacity:.92;
      pointer-events:none !important;
    }
    .tile-resize-placeholder.show { display:block; }
    .tile-resize-placeholder.invalid::after {
      border-color:#ef4444;
      background:rgba(239,68,68,0.16);
      box-shadow:0 0 0 2px rgba(239,68,68,0.18) inset;
    }
    .tile-resize-handle {
      position:absolute;
      opacity:0;
      pointer-events:none;
      transition:opacity 0.15s ease;
      z-index:30;
      background:rgba(255,255,255,0.16);
      border:1px solid rgba(255,255,255,0.24);
      box-shadow:0 1px 4px rgba(15,23,42,0.18);
    }
    .tile.active .tile-resize-handle,
    .tile[data-selected="1"] .tile-resize-handle {
      opacity:0.22;
      pointer-events:auto;
    }
    .tile.active:hover .tile-resize-handle,
    .tile[data-selected="1"]:hover .tile-resize-handle,
    .tile.resizing .tile-resize-handle {
      opacity:0.58;
      pointer-events:auto;
    }
    .tile-resize-handle-e {
      top:50%;
      right:-4px;
      width:8px;
      height:34px;
      transform:translateY(-50%);
      border-radius:999px;
      cursor:ew-resize;
    }
    .tile-resize-handle-s {
      left:50%;
      bottom:-4px;
      width:34px;
      height:8px;
      transform:translateX(-50%);
      border-radius:999px;
      cursor:ns-resize;
    }
    .tile-resize-handle-se {
      right:-2px;
      bottom:-2px;
      width:14px;
      height:14px;
      border-radius:6px;
      cursor:nwse-resize;
    }
    .tile.active:hover,
    .tile[data-selected="1"]:hover { opacity:1; filter:none; }
    .tile.empty { background:transparent !important; border:3px solid transparent; }
    .tile.empty.active,
    .tile.empty[data-selected="1"] { border:3px solid #26a69a; box-shadow:0 0 12px rgba(38,166,154,0.55); }
    .tile.empty:hover:not(.active) { border-color:rgba(38,166,154,0.4); }
    .tile-title {
      color:#fff;
      font-weight:normal;
      font-size:var(--fs20, 10px);
      text-align:left;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
      align-self:start;
    }

    /* Tile Icons (MDI) */
    .tile-icon {
      color:#fff;
      font-size:var(--icon-size, 24px);
      line-height:1;
    }
    /* Zentrierter Geist-Hinweis fuer Kacheln ohne Live-Vorschau (Wetter/Media) */
    .tile-ghost-icon {
      position:absolute;
      inset:0;
      display:flex;
      align-items:center;
      justify-content:center;
      pointer-events:none;
      color:rgba(255,255,255,0.35);
    }
    .tile-ghost-icon .mdi { font-size:var(--icon-size, 24px); line-height:1; }

    /* Settings Panel: sticky rechts, scrollt bei langem Inhalt intern
       (max. Viewport-Hoehe) statt die ganze Seite zu strecken. */
    .tile-settings {
      flex:0 1 var(--settings-panel-width, 390px);
      width:var(--settings-panel-width, 390px);
      min-width:300px;
      max-width:var(--settings-panel-width, 390px);
      margin-left:auto;
      box-sizing:border-box;
      display:flex;
      flex-direction:column;
      background:var(--panel);
      border:1px solid #232323;
      border-radius:18px;
      padding:20px;
      position:sticky;
      top:20px;
      max-height:calc(100vh - 40px);
      overflow:hidden;
    }
    .tile-settings.hidden { display:none; }
    /* Kopf (Titel + Typ) bleibt stehen, nur der Body scrollt */
    .tile-specific-settings { display:flex; flex:1 1 auto; flex-direction:column; min-height:0; }
    .screensaver-background-settings { display:flex; flex:1 1 auto; flex-direction:column; min-height:0; }
    .screensaver-background-settings.hidden { display:none; }
    .screensaver-background-settings > .tile-settings-body {
      flex:1 1 auto;
      min-height:0;
      overflow-y:auto;
    }
    .tile-settings-head { flex:0 0 auto; }
    .tile-settings-body {
      flex:1 1 auto;
      min-height:0;
      overflow-y:auto;
      overscroll-behavior:contain;
      margin-right:-12px;
      padding-right:12px;
    }
    /* Die Aktionen bleiben im Desktop-Panel jederzeit erreichbar. */
    .tile-specific-settings > .tile-actions {
      flex:0 0 auto;
      margin:10px 0 0;
      padding-top:10px;
      border-top:1px solid #232323;
    }
    /* Kompaktere Controls nur im Panel */
    .tile-settings label { font-size:12px; margin-bottom:4px; }
    .tile-settings input, .tile-settings select, .tile-settings textarea {
      padding:9px 12px;
      font-size:13px;
      border-radius:10px;
      margin-bottom:10px;
    }
    .tile-settings select { background-position:right 12px center; padding-right:32px; }
    .tile-settings input[type="range"] { padding:0; }
    .tile-settings h3 { margin:0 0 14px; color:var(--text); font-size:17px; }
    .tile-settings .hidden { display:none; }
    /* Typ-Felder: kompakte zweispaltige Zeilen (Label links, Feld rechts) */
    .type-fields { display:none; margin-top:10px; }
    .type-fields.show {
      display:grid;
      grid-template-columns:minmax(0,1fr) minmax(0,1.35fr);
      gap:9px 10px;
      align-items:center;
    }
    .type-fields.show > label { margin:0; }
    .type-fields.show > input, .type-fields.show > select { margin:0; }
    .type-fields.show > div, .type-fields.show > textarea { grid-column:1 / -1; }
    .type-fields.show > select[id*="_entity"],
    .type-fields.show > select[id$="_scene_alias"],
    .type-fields.show > select[id$="_animation_file"] { grid-column:1 / -1; }
    .gauge-fields, .graph-fields {
      display:grid;
      grid-template-columns:minmax(0,1fr) minmax(0,1.35fr);
      gap:9px 10px;
      align-items:center;
    }
    .gauge-fields > label, .graph-fields > label,
    .gauge-fields > input, .graph-fields > input { margin:0; }
    .tile-color-label-row,
    .tile-color-row { display:grid; grid-template-columns:minmax(72px,1fr) 38px; gap:8px; align-items:center; }
    .tile-color-label-row.has-opacity,
    .tile-color-row.has-opacity { grid-template-columns:minmax(72px,1fr) minmax(110px,1.5fr) 38px; }
    .tile-color-label-row { margin-top:13px; margin-bottom:4px; }
    .tile-color-label-row span { color:var(--text-2); font-size:12px; font-weight:600; line-height:14px; }
    .tile-color-row { margin-bottom:10px; }
    .tile-color-row input[type="color"] { width:100%; min-width:0; height:38px; margin:0; padding:4px 7px; border-radius:10px; }
    .tile-color-row input[type="range"] { width:100%; min-width:0; margin:0; padding:0; align-self:center; }
    .tile-color-reset-btn {
      flex:0 0 38px;
      width:38px;
      height:38px;
      display:inline-flex;
      align-items:center;
      justify-content:center;
      border:1px solid var(--line-strong);
      border-radius:10px;
      background:var(--well);
      color:var(--text-2);
      cursor:pointer;
      font-size:17px;
      transition:border-color 0.15s, color 0.15s;
    }
    .tile-color-reset-btn:hover { border-color:#4a4a4a; color:var(--text); }
    .tile-layout {
      display:grid;
      grid-template-columns:repeat(4, minmax(0, 1fr));
      gap:8px;
      margin:12px 0 12px;
    }
    .layout-field label { margin-bottom:4px; font-size:11px; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .layout-field input { padding:8px 10px; font-size:13px; margin-bottom:0; }
    .inline-checkbox { display:flex; align-items:center; gap:8px; font-weight:600; margin-top:8px; }
    .inline-checkbox input { width:auto; padding:0; margin:0; flex:0 0 auto; }
    .clock-toggle-row { display:grid; grid-template-columns:repeat(2, minmax(120px, max-content)); gap:8px 18px; align-items:center; justify-content:start; margin-bottom:8px; }
    .clock-toggle-row .inline-checkbox { display:inline-flex; width:auto; margin-top:0; white-space:nowrap; }

    /* Screensaverspezifische Felder innerhalb des normalen Tile-Editors. */
    .screensaver-fixed-type { display:grid; grid-template-columns:1fr 1.35fr; gap:10px; align-items:center; margin-bottom:10px; }
    .screensaver-fixed-type label { margin:0; }
    .screensaver-fixed-type input { margin:0; }
    .screensaver-wallpaper-heading { color:var(--text-2); font-weight:600; font-size:13px; letter-spacing:normal; text-transform:none; margin:18px 0 6px; }
    .screensaver-storage-hint { color:var(--text-2); font-size:12px; line-height:1.4; margin:-2px 0 10px; }
    .screensaver-wallpaper-list { display:flex; flex-direction:column; gap:7px; max-height:190px; overflow:auto; }
    .screensaver-wallpaper-row { display:grid; grid-template-columns:auto 1fr auto auto; align-items:center; gap:8px; padding:7px 9px; border:1px solid var(--line); border-radius:10px; background:var(--well); }
    .screensaver-wallpaper-row.active { border-color:#26b5aa; }
    .screensaver-wallpaper-row input[type="checkbox"] { width:16px; height:16px; padding:0; margin:0; }
    .screensaver-wallpaper-name { overflow:hidden; text-overflow:ellipsis; white-space:nowrap; cursor:pointer; font-size:13px; color:var(--text); }
    /* Ghost-Button wie .tile-color-reset-btn, damit die Reihenfolge-Buttons
       nicht als native Browser-Buttons mit fremder Schrift auffallen. */
    .screensaver-wallpaper-move {
      width:28px;
      height:28px;
      padding:0;
      margin:0;
      display:inline-flex;
      align-items:center;
      justify-content:center;
      border:1px solid var(--line-strong);
      border-radius:8px;
      background:var(--well);
      color:var(--text-2);
      font-family:inherit;
      cursor:pointer;
      transition:border-color 0.15s, color 0.15s;
    }
    .screensaver-wallpaper-move:hover:not(:disabled) { border-color:#4a4a4a; color:var(--text); }
    .screensaver-wallpaper-move:disabled { opacity:0.35; cursor:default; }
    .screensaver-wallpaper-move svg { display:block; }
    .screensaver-wallpaper-controls { margin-top:12px; }
    .screensaver-focus-grid, .screensaver-two-fields { display:grid; grid-template-columns:1fr 1fr; gap:10px; }
    .screensaver-two-fields label { margin:0; }
    .screensaver-two-fields input, .screensaver-two-fields select { margin-top:5px; margin-bottom:0; }
    .screensaver-save-state { min-height:20px; margin-top:12px; color:var(--text-2); font-size:12px; }

    /* Schmales Fenster: Settings-Panel rueckt unter die Grid-Vorschau,
       das Grid selbst scrollt bei Bedarf horizontal (.tile-grid-scroll). */
    @media (max-width: 1180px) {
      .tile-editor { flex-direction:column; align-items:stretch; }
      .tile-editor-main { flex:1 1 auto; }
      .tile-settings { flex:1 1 auto; width:auto; min-width:0; max-width:none; margin-left:0; position:static; max-height:none; }
    }
    @media (max-width: 780px) {
      body { height:auto; overflow:auto; }
      .wrapper { height:auto; padding:16px; }
      .card { height:auto; min-height:calc(100vh - 32px); }
      #tab-network.active { flex:none; overflow:visible; }
      #admin_settings_form {
        flex:none;
        overflow:visible;
        margin:0;
        padding:0;
      }
      .admin-footer-actions {
        position:sticky;
        bottom:0;
        z-index:5;
        padding:12px 0;
      }
      .settings-grid { grid-template-columns:1fr; }
      .settings-subgrid { grid-template-columns:1fr; }
      .settings-full { grid-column:auto; }
      .file-manager-topbar { align-items:stretch; flex-direction:column; }
      .file-manager-toolbar-group,
      .file-manager-upload-row { align-items:flex-start; }
      .file-manager-selection-bar { align-items:stretch; flex-direction:column; }
      .file-manager-selection-actions { justify-content:flex-start; }
    }
    .gauge-fields { padding-left:4px; }
    .gauge-fields.hidden { display:none; }

    /* Notification */
    .notification {
      position:fixed;
      bottom:24px;
      right:24px;
      background:#26a69a;
      color:#fff;
      padding:16px 24px;
      border-radius:14px;
      box-shadow:0 10px 30px rgba(0,0,0,0.45);
      font-weight:600;
      opacity:0;
      transform:translateY(20px);
      transition:all 0.3s;
      z-index:1000;
    }
    .notification.show { opacity:1; transform:translateY(0); }
    body.tile-resize-active { user-select:none; cursor:default; }
  </style>
)html";

  append_tile_type_styles(html);
}
