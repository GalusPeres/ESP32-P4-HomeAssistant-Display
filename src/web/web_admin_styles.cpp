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
    .settings-actions { display:flex; gap:10px; flex-wrap:wrap; }
    .settings-actions .btn { width:auto; }
    .file-picker { display:flex; gap:10px; align-items:center; flex-wrap:wrap; margin-top:8px; }
    .file-picker-name { color:#475569; font-size:14px; overflow-wrap:anywhere; }
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
    .file-manager { grid-template-columns:1fr; align-items:start; }
    .file-manager-topbar { display:flex; align-items:center; justify-content:space-between; gap:12px; flex-wrap:wrap; }
    .file-manager-storage-state { display:inline-flex; align-items:center; min-height:28px; padding:4px 10px; border-radius:999px; background:#e2e8f0; color:#475569; font-size:14px; font-weight:700; white-space:nowrap; }
    .file-manager-storage-state.ok { background:#dcfce7; color:#15803d; }
    .file-manager-storage-state.error { background:#fee2e2; color:#b91c1c; }
    .file-manager-storage-state.checking { background:#e0f2fe; color:#0369a1; }
    .file-manager-toolbar-group,
    .file-manager-upload-row { display:flex; align-items:center; gap:8px; flex-wrap:wrap; min-height:38px; }
    .file-manager-upload-row { margin-top:8px; }
    .file-manager .file-manager-toolbar-btn,
    .file-manager .file-manager-selection-btn { display:inline-flex; align-items:center; justify-content:center; width:auto; min-width:0; height:36px; margin:0; padding:0 12px; border-radius:8px; font-size:14px; line-height:1.2; flex:0 0 auto; justify-self:start; }
    .file-manager-upload-row .file-picker-name { flex:1 1 260px; min-width:220px; color:#64748b; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
    .file-manager-status { min-height:18px; margin-top:6px; }
    .file-manager-status.error { color:#dc2626; }
    .file-manager-status.success { color:#16a34a; }
    .file-manager-selection-bar { display:flex; align-items:center; justify-content:space-between; gap:10px; margin-top:8px; padding:8px 10px; border:1px solid #dbe4f3; border-radius:8px; background:#f8fafc; }
    .file-manager-selection-info { min-width:0; color:#334155; font-size:13px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
    .file-manager-selection-actions { display:flex; align-items:center; justify-content:flex-end; gap:6px; flex-wrap:wrap; }
    .file-manager .file-manager-selection-btn:disabled,
    .file-manager .file-manager-toolbar-btn:disabled { opacity:.45; cursor:not-allowed; }
    .file-manager-breadcrumb { display:flex; align-items:center; gap:4px; min-height:34px; margin:0 0 6px; padding:6px 8px; border:1px solid #dbe4f3; border-radius:8px; background:#f8fafc; overflow-x:auto; white-space:nowrap; }
    .file-manager-breadcrumb-item { display:inline-flex; align-items:center; gap:5px; padding:3px 5px; border:0; border-radius:6px; background:transparent; color:#2563eb; font:inherit; font-size:13px; font-weight:600; cursor:pointer; }
    .file-manager-breadcrumb-item:hover:not(:disabled) { background:#e0edff; }
    .file-manager-breadcrumb-item:disabled { color:#334155; cursor:default; }
    .file-manager-breadcrumb-separator { color:#94a3b8; font-size:13px; }
    .file-manager-table-wrap { width:100%; max-height:520px; overflow:auto; border:1px solid #dbe4f3; border-radius:8px; background:#fff; }
    .file-manager-table { width:100%; border-collapse:collapse; font-size:13px; color:#0f172a; table-layout:fixed; }
    .file-manager-table th,
    .file-manager-table td { padding:8px 10px; border-bottom:1px solid #e5edf8; text-align:left; vertical-align:middle; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .file-manager-table th { position:sticky; top:0; z-index:1; background:#f1f5f9; color:#475569; font-size:12px; font-weight:700; text-transform:none; letter-spacing:0; }
    .file-manager-table tbody tr { cursor:pointer; outline:none; }
    .file-manager-table tbody tr:nth-child(odd) { background:#fff; }
    .file-manager-table tbody tr:nth-child(even) { background:#f8fafc; }
    .file-manager-table tbody tr:hover { background:#eef6ff; }
    .file-manager-table tbody tr:focus { box-shadow:inset 0 0 0 2px #93c5fd; }
    .file-manager-table tbody tr.file-manager-row-selected td { background:#dbeafe !important; }
    .file-manager-table tr:last-child td { border-bottom:none; }
    .file-manager-table th:nth-child(1), .file-manager-table td:nth-child(1) { width:auto; }
    .file-manager-table th:nth-child(2), .file-manager-table td:nth-child(2) { width:170px; }
    .file-manager-table th:nth-child(3), .file-manager-table td:nth-child(3) { width:110px; }
    .file-manager-name { display:flex; align-items:center; gap:8px; min-width:0; overflow:hidden; text-overflow:ellipsis; }
    .file-manager-name-icon { flex:0 0 auto; width:18px; color:#64748b; font-size:17px; text-align:center; }
    .file-manager-name span,
    .file-manager-name-link { min-width:0; overflow:hidden; text-overflow:ellipsis; }
    .file-manager-name-link { padding:0; border:0; background:transparent; color:#2563eb; font:inherit; font-weight:600; text-align:left; cursor:pointer; }
    .file-manager-name-link:hover { text-decoration:underline; }
    .file-manager-folder-name { color:#2563eb; font-weight:600; }
    .file-manager-parent-row .file-manager-name-icon { color:#2563eb; }
    .file-manager-muted { color:#64748b; }
    .file-manager-size-cell { color:#334155; font-family:Consolas, 'Courier New', monospace; }
    .file-manager-parent-row td { background:#f1f5f9 !important; }
    .ota-version-value {
      display:inline-block;
      font-family:Consolas, 'Courier New', monospace;
      overflow-wrap:anywhere;
      word-break:break-word;
    }
    .ota-status { min-height:18px; margin-top:8px; }
    .ota-status.error { color:#dc2626; }
    .ota-status.success { color:#16a34a; }
    .ota-progress {
      width:100%;
      height:10px;
      margin-top:8px;
      border-radius:999px;
      overflow:hidden;
      background:#dbe4f3;
    }
    .ota-progress.active::before {
      content:'';
      display:block;
      width:40%;
      height:100%;
      border-radius:999px;
      background:linear-gradient(90deg, #4f46e5 0%, #3b82f6 100%);
      animation:ota-progress-slide 1.1s ease-in-out infinite;
    }
    .ota-progress-bar {
      width:0%;
      height:100%;
      border-radius:999px;
      background:linear-gradient(90deg, #4f46e5 0%, #3b82f6 100%);
      transition:width 0.18s ease;
    }
    @keyframes ota-progress-slide {
      0% { transform:translateX(-100%); }
      100% { transform:translateX(250%); }
    }
    .settings-checkbox { display:flex; align-items:center; gap:10px; margin:0; font-size:14px; color:#0f172a; }
    .settings-checkbox input { width:auto; padding:0; margin:0; }
    .is-hidden { display:none !important; }
    .btn { padding:12px 18px; border:none; border-radius:10px; background:#4f46e5; color:#fff; font-size:16px; cursor:pointer; transition:background 0.2s; }
    .btn:hover { background:#4338ca; }
    .btn-secondary { background:#94a3b8; margin-top:12px; width:100%; }
    .btn-danger { background:#ef4444; }
    .btn-danger:hover { background:#dc2626; }
    .admin-hidden-form { display:none; }
    .admin-footer-actions { display:flex; align-items:center; justify-content:flex-end; gap:12px; flex-wrap:wrap; margin-top:24px; }
    .admin-footer-actions .btn { width:auto; min-width:180px; margin-top:0; padding:10px 16px; border-radius:8px; font-size:14px; }
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
    .inline-checkbox input { width:auto; padding:0; margin:0; flex:0 0 auto; }
    .clock-toggle-row { display:grid; grid-template-columns:repeat(2, minmax(120px, max-content)); gap:8px 18px; align-items:center; justify-content:start; margin-bottom:8px; }
    .clock-toggle-row .inline-checkbox { display:inline-flex; width:auto; margin-top:0; white-space:nowrap; }

    @media (max-width: 780px) {
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
