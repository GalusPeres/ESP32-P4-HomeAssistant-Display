#include "src/web/web_admin_styles.h"
#include "src/types/types_registry.h"
#include "src/tiles/tile_config.h"

namespace {

constexpr int kPreviewTargetHeight = 430;
constexpr int kPreviewPad = 12;
constexpr int kPreviewGap = 10;

int preview_pad_px() {
  return kPreviewPad;
}

int preview_gap_px() {
  return kPreviewGap;
}

int preview_cell_h_px() {
  const int pad = preview_pad_px();
  const int gap = preview_gap_px();
  int cell = (kPreviewTargetHeight - (2 * pad) - (gap * (GRID_ROWS - 1))) / GRID_ROWS;
  return (cell < 40) ? 40 : cell;
}

int preview_cell_w_px() {
  const int cell_h = preview_cell_h_px();
  int cell_w = (GRID_CELL_W * cell_h + (GRID_CELL_H / 2)) / GRID_CELL_H;
  return (cell_w < 40) ? 40 : cell_w;
}

}  // namespace

void appendAdminStyles(String& html) {
  const int preview_pad = preview_pad_px();
  const int preview_gap = preview_gap_px();
  const int preview_cell_w = preview_cell_w_px();
  const int preview_cell_h = preview_cell_h_px();
  html += R"html(
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@mdi/font@7.4.47/css/materialdesignicons.min.css">
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; background:#eef2ff; margin:0; padding:0; }
    .wrapper { max-width:1200px; margin:20px auto; padding:20px; }
    .card { background:#fff; border-radius:16px; box-shadow:0 20px 45px rgba(15,23,42,0.15); padding:32px; }
    h1 { margin:0 0 8px; font-size:28px; color:#1e293b; }
    .subtitle { color:#475569; margin-bottom:24px; }
    .status { display:grid; grid-template-columns:repeat(auto-fit,minmax(220px,1fr)); gap:12px; margin-bottom:28px; }
    .status div { background:#f8fafc; border-radius:12px; padding:14px; border:1px solid #e2e8f0; }
    .status-label { font-size:12px; text-transform:uppercase; color:#64748b; letter-spacing:.08em; }
    .status-value { font-size:16px; color:#0f172a; font-weight:600; }

    /* Tab Navigation */
    .tab-nav { display:flex; gap:8px; margin-bottom:24px; border-bottom:2px solid #e2e8f0; }
    .tab-btn {
      padding:12px 20px;
      border:none;
      background:transparent;
      color:#64748b;
      font-size:15px;
      font-weight:600;
      cursor:pointer;
      border-bottom:3px solid transparent;
      transition:all 0.3s;
      display:flex;
      flex-direction:row;
      align-items:center;
      justify-content:center;
      gap:8px;
    }
    .tab-btn:hover { color:#4f46e5; background:#f8fafc; }
    .tab-btn.active { color:#4f46e5; border-bottom-color:#4f46e5; }
    .edit-icon { font-size:12px; margin-left:6px; opacity:0.5; cursor:pointer; transition:opacity 0.2s; }
    .edit-icon:hover { opacity:1; }
    .tab-content { display:none; }
    .tab-content.active { display:block; }

    form { display:grid; gap:16px; margin-bottom:32px; }
    label { font-size:13px; font-weight:600; color:#475569; display:block; margin-bottom:6px; }
    input { width:100%; padding:12px; border:1px solid #cbd5f5; border-radius:10px; font-size:15px; box-sizing:border-box; }
    select { max-width:100%; }
    .password-field { display:flex; gap:8px; align-items:center; }
    .password-field input { flex:1 1 auto; }
    .password-toggle { flex:0 0 auto; padding:12px 14px; border:1px solid #cbd5f5; border-radius:10px; background:#fff; color:#334155; cursor:pointer; font-size:13px; font-weight:600; }
    .password-toggle:hover { background:#f8fafc; }
    .settings-section { background:#f8fafc; border:1px solid #e2e8f0; border-radius:12px; padding:16px; }
    .settings-grid { display:grid; grid-template-columns:repeat(2, minmax(0, 1fr)); gap:16px; }
    .settings-subgrid { display:grid; grid-template-columns:repeat(2, minmax(0, 1fr)); gap:16px; }
    .settings-full { grid-column:1 / -1; }
    .settings-note { font-size:12px; color:#64748b; margin-top:4px; }
    .settings-checkbox { display:flex; align-items:center; gap:10px; margin:0; font-size:14px; color:#0f172a; }
    .settings-checkbox input { width:auto; padding:0; margin:0; }
    .is-hidden { display:none !important; }
    .btn { padding:12px 18px; border:none; border-radius:10px; background:#4f46e5; color:#fff; font-size:16px; cursor:pointer; transition:background 0.2s; }
    .btn:hover { background:#4338ca; }
    .btn-secondary { background:#94a3b8; margin-top:12px; width:100%; }
    .btn-danger { background:#ef4444; }
    .btn-danger:hover { background:#dc2626; }
    .section-title { margin:32px 0 12px; text-transform:uppercase; font-size:12px; letter-spacing:.1em; color:#a1a1aa; }
    .settings-section .section-title { margin:0 0 12px; }
    .hint { color:#64748b; font-size:14px; margin:8px 0 16px; }
    .list-block { background:#f8fafc; border-radius:12px; padding:16px; border:1px solid #e2e8f0; }
    .list-block strong { display:block; margin:12px 0 6px; color:#1e293b; }
    .list { list-style:none; padding-left:18px; margin:0; }
    .list li { padding:4px 0; font-family:monospace; color:#0f172a; }
    .layout-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:16px; }
    .slot { background:#f8fafc; border:1px solid #e2e8f0; border-radius:12px; padding:12px; }
    .slot-scene { background:#fff7ed; border-color:#fed7aa; }
    .slot-label { font-size:13px; font-weight:600; color:#475569; margin-bottom:8px; }
    .slot select, .slot input { width:100%; box-sizing:border-box; }
    .slot select { padding:10px; border:1px solid #cbd5f5; border-radius:10px; font-size:15px; background:#fff; margin-bottom:8px; }
    .slot input { padding:9px; border:1px solid #d6defa; border-radius:10px; font-size:13px; margin-bottom:6px; }
    .legacy-block { border:1px dashed #cbd5f5; background:#f8fafc; border-radius:12px; padding:12px; margin-bottom:16px; }

    /* Tab Settings Above Grid */
    .tab-settings-top {
      background:#f8fafc;
      border-radius:12px;
      padding:16px;
      margin-bottom:20px;
      border:1px solid #e2e8f0;
      max-width:600px;
    }

    .tile-editor { display:grid; grid-template-columns:auto 350px; gap:24px; align-items:start; }
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
      background:#000;
      border-radius:8px;
      width:fit-content;
      height:fit-content;
    }

    /* Display-aehnliche Kacheln (50% Skalierung) */
    /* Display: Title=TOP_LEFT, Value=CENTER(-30,18), Unit=RIGHT_MID */
    .tile {
      background:#2A2A2A;
      border-radius:11px;
      cursor:pointer;
      border:3px solid transparent;
      padding:8px;
      position:relative;
      box-sizing:border-box;
      overflow:hidden;
      background-clip:padding-box;
      clip-path: inset(0 round 11px);
    }
    .tile:hover:not(.active) {
      border:3px dashed rgba(74,158,255,0.6);
      box-shadow:0 0 0 2px rgba(74,158,255,0.12) inset;
      border-radius:11px;
      background-clip:padding-box;
      clip-path: inset(0 round 11px);
    }
    .tile.active {
      border:3px solid #4A9EFF;
      box-shadow:0 0 12px rgba(74,158,255,0.6);
      border-radius:11px;
      background-clip:padding-box;
      clip-path: inset(0 round 11px);
    }
    .tile.dragging {
      opacity:0.02;
      border:3px solid transparent;
      box-shadow:none;
      border-radius:11px;
      background-clip:padding-box;
      clip-path: inset(0 round 11px);
    }
    .tile.resizing {
      z-index:24;
      box-shadow:0 0 0 2px rgba(74,158,255,0.25) inset, 0 0 18px rgba(74,158,255,0.18);
    }
    .tile.resize-invalid {
      box-shadow:0 0 0 2px rgba(239,68,68,0.22) inset, 0 0 18px rgba(239,68,68,0.2);
    }
    .tile.reflow-preview {
      border:3px solid rgba(125,211,252,0.55);
      box-shadow:0 0 0 2px rgba(125,211,252,0.35) inset, 0 0 18px rgba(56,189,248,0.18);
      filter:brightness(1.06);
    }
    .tile.drop-target {
       border:3px dashed #4A9EFF;
       background:rgba(74,158,255,0.12);
       box-shadow:0 0 0 2px rgba(74,158,255,0.2) inset;
       border-radius:11px;
       background-clip:padding-box;
       clip-path: inset(0 round 11px);
    }
    .tile.empty.drop-target {
       border:3px dashed #4A9EFF;
       background:rgba(74,158,255,0.08);
    }
    .tile-drop-placeholder {
      display:none;
      border:3px dashed #4A9EFF;
      background:rgba(74,158,255,0.18);
      box-shadow:0 0 0 2px rgba(74,158,255,0.2) inset;
      border-radius:11px;
      box-sizing:border-box;
      pointer-events:none;
      z-index:20;
      clip-path: inset(0 round 11px);
    }
    .tile-drop-placeholder.show { display:block; }
    .tile-drop-placeholder.invalid {
      border-color:#ef4444;
      background:rgba(239,68,68,0.16);
      box-shadow:0 0 0 2px rgba(239,68,68,0.18) inset;
    }
    .tile-resize-placeholder {
      display:none;
      border:3px dashed #ef4444;
      background:rgba(239,68,68,0.14);
      box-shadow:0 0 0 2px rgba(239,68,68,0.18) inset;
      border-radius:11px;
      box-sizing:border-box;
      pointer-events:none;
      z-index:22;
      clip-path: inset(0 round 11px);
    }
    .tile-resize-placeholder.show { display:block; }
    .tile-resize-placeholder.invalid {
      border-color:#ef4444;
      background:rgba(239,68,68,0.16);
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
    .tile.active .tile-resize-handle {
      opacity:0.22;
      pointer-events:auto;
    }
    .tile.active:hover .tile-resize-handle,
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
    .tile.active:hover { opacity:1; filter:none; }
    .tile.empty { background:transparent !important; border:3px solid transparent; }
    .tile.empty.active { border:3px solid #4A9EFF; box-shadow:0 0 12px rgba(74,158,255,0.6); }
    .tile.empty:hover:not(.active) { border-color:rgba(74,158,255,0.4); }
    .tile-title {
      color:#fff;
      font-weight:normal;
      font-size:12px;
      text-align:left;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
      align-self:start;
    }

    /* Tile Icons (MDI) */
    .tile-icon {
      color:#fff;
      font-size:24px;
      line-height:1;
    }
    /* Settings Panel */
    .tile-settings {
      background:#f8fafc;
      border-radius:12px;
      padding:20px;
      height:fit-content;
      position:sticky;
      top:20px;
    }
    .tile-settings.hidden { display:none; }
    .tile-settings input, .tile-settings select { margin-bottom:12px; }
    .tile-settings h3 { margin:0 0 16px; color:#1e293b; font-size:18px; }
    .type-fields { display:none; margin-top:12px; }
    .type-fields.show { display:block; }
    .tile-layout {
      display:grid;
      grid-template-columns:repeat(2, minmax(0, 1fr));
      gap:10px;
      margin:12px 0 12px;
    }
    .layout-field label { margin-bottom:4px; font-size:12px; }
    .layout-field input { padding:10px; }
    .inline-checkbox { display:flex; align-items:center; gap:8px; font-weight:600; margin-top:8px; }
    .inline-checkbox input { margin:0; }

    @media (max-width: 780px) {
      .settings-grid { grid-template-columns:1fr; }
      .settings-subgrid { grid-template-columns:1fr; }
      .settings-full { grid-column:auto; }
    }
    .gauge-fields { padding-left:4px; }
    .gauge-fields.hidden { display:none; }

    /* Notification */
    .notification {
      position:fixed;
      bottom:24px;
      right:24px;
      background:#10b981;
      color:#fff;
      padding:16px 24px;
      border-radius:12px;
      box-shadow:0 10px 30px rgba(0,0,0,0.2);
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
