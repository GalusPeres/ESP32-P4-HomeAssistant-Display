  function clampInt(value, min, max, fallback) {
    const v = parseInt(value, 10);
    if (isNaN(v)) return fallback !== undefined ? fallback : min;
    if (v < min) return min;
    if (v > max) return max;
    return v;
  }

  function normalizeLayoutForTileType(typeValue, col, row, spanW, spanH) {
    let safeCol = clampInt(col, 0, GRID_COLS - 1, 0);
    let safeRow = clampInt(row, 0, GRID_ROWS - 1, 0);
    let safeW = clampInt(spanW, 1, GRID_COLS, 1);
    let safeH = clampInt(spanH, 1, GRID_ROWS, 1);
    if (Number(typeValue) === MEDIA_TILE_TYPE) {
      const minW = Math.min(MEDIA_TILE_MIN_SPAN, GRID_COLS);
      const minH = Math.min(MEDIA_TILE_MIN_SPAN, GRID_ROWS);
      safeW = clampInt(safeW, minW, Math.min(MEDIA_TILE_MAX_SPAN, GRID_COLS), minW);
      safeH = clampInt(safeH, minH, Math.min(MEDIA_TILE_MAX_SPAN, GRID_ROWS), minH);
      safeCol = Math.min(safeCol, GRID_COLS - safeW);
      safeRow = Math.min(safeRow, GRID_ROWS - safeH);
    } else {
      safeW = Math.min(safeW, GRID_COLS - safeCol);
      safeH = Math.min(safeH, GRID_ROWS - safeRow);
    }
    return { col: safeCol, row: safeRow, span_w: safeW, span_h: safeH };
  }

  function constrainLayoutToTab(layout, tab) {
    const firstRow = firstAllowedGridRow(tab);
    if (layout.row < firstRow) layout.row = firstRow;
    if (layout.span_h > GRID_ROWS - layout.row) {
      layout.span_h = GRID_ROWS - layout.row;
    }
    return layout;
  }

  function normalizeTileLayout(tile, index, tab = currentTileTab) {
    const fallbackCol = index % GRID_COLS;
    const firstRow = firstAllowedGridRow(tab);
    const fallbackRow = Math.max(firstRow, Math.floor(index / GRID_COLS));
    const col = clampInt(tile?.col, 0, GRID_COLS - 1, fallbackCol);
    const row = clampInt(tile?.row, firstRow, GRID_ROWS - 1, fallbackRow);
    let spanW = clampInt(tile?.span_w, 1, GRID_COLS, 1);
    let spanH = clampInt(tile?.span_h, 1, GRID_ROWS, 1);
    return constrainLayoutToTab(
      normalizeLayoutForTileType(tile?.type, col, row, spanW, spanH), tab);
  }

  function setGridItemPosition(el, col, row, spanW, spanH) {
    if (!el) return;
    el.style.gridColumn = (col + 1) + ' / span ' + spanW;
    el.style.gridRow = (row + 1) + ' / span ' + spanH;
    el.dataset.col = String(col);
    el.dataset.row = String(row);
    el.dataset.spanW = String(spanW);
    el.dataset.spanH = String(spanH);
  }

  function setTileGridPosition(el, col, row, spanW, spanH) {
    setGridItemPosition(el, col, row, spanW, spanH);
  }

  function getTileElementLayout(tab, index) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return null;
    const col = clampInt(el.dataset.col, 0, GRID_COLS - 1, null);
    const row = clampInt(el.dataset.row, firstAllowedGridRow(tab), GRID_ROWS - 1, null);
    const spanW = clampInt(el.dataset.spanW, 1, GRID_COLS, null);
    const spanH = clampInt(el.dataset.spanH, 1, GRID_ROWS, null);
    if (col === null || row === null || spanW === null || spanH === null) return null;
    return { col, row, span_w: spanW, span_h: spanH };
  }

  function layoutTiles(tab, tiles) {
    if (!Array.isArray(tiles)) return;
    const occupied = Array.from({ length: GRID_ROWS }, () => Array(GRID_COLS).fill(false));
    const emptyIndices = [];

    tiles.forEach((tile, idx) => {
      const typeNum = Number(tile?.type);
      if (!tile || isNaN(typeNum) || typeNum === 0) {
        emptyIndices.push(idx);
        return;
      }
      const layout = normalizeTileLayout(tile, idx, tab);
      const el = document.getElementById(tab + '-tile-' + idx);
      if (el) {
        setTileGridPosition(el, layout.col, layout.row, layout.span_w, layout.span_h);
        el.style.display = '';
      }
      for (let r = layout.row; r < layout.row + layout.span_h; r++) {
        for (let c = layout.col; c < layout.col + layout.span_w; c++) {
          if (r < GRID_ROWS && c < GRID_COLS) occupied[r][c] = true;
        }
      }
    });

    const freeCells = [];
    for (let r = firstAllowedGridRow(tab); r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        if (!occupied[r][c]) freeCells.push({ col: c, row: r });
      }
    }

    emptyIndices.forEach((idx, i) => {
      const el = document.getElementById(tab + '-tile-' + idx);
      if (!el) return;
      if (i < freeCells.length) {
        const cell = freeCells[i];
        setTileGridPosition(el, cell.col, cell.row, 1, 1);
        el.style.display = '';
      } else {
        el.style.display = 'none';
      }
    });
  }

  function syncTileGridStructure(tab, tiles) {
    if (!Array.isArray(tiles)) return;
    tiles.forEach((tile, index) => {
      const el = document.getElementById(tab + '-tile-' + index);
      if (!el) return;
      el.dataset.index = String(index);
      el.dataset.type = String(tile?.type ?? 0);
    });
    layoutTiles(tab, tiles);
  }

  function normalizeLayoutInputs(tab) {
    const prefix = tab;
    const colEl = document.getElementById(prefix + '_tile_col');
    const rowEl = document.getElementById(prefix + '_tile_row');
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    const spanHEl = document.getElementById(prefix + '_tile_span_h');

    if (!colEl || !rowEl || !spanWEl || !spanHEl) {
      const fallback = getTileElementLayout(tab, currentTileIndex);
      if (fallback) return fallback;
      return { col: 0, row: 0, span_w: 1, span_h: 1 };
    }

    let col = clampInt(colEl.value, 1, GRID_COLS, 1);
    const firstRow = firstAllowedGridRow(tab);
    let row = clampInt(rowEl.value, firstRow + 1, GRID_ROWS, firstRow + 1);
    let spanW = clampInt(spanWEl.value, 1, GRID_COLS, 1);
    let spanH = clampInt(spanHEl.value, 1, GRID_ROWS, 1);

    const typeValue = document.getElementById(prefix + '_tile_type')?.value || '0';
    const layout = constrainLayoutToTab(
      normalizeLayoutForTileType(typeValue, col - 1, row - 1, spanW, spanH),
      tab);
    col = layout.col + 1;
    row = layout.row + 1;
    spanW = layout.span_w;
    spanH = layout.span_h;

    colEl.value = String(col);
    rowEl.value = String(row);
    spanWEl.value = String(spanW);
    spanHEl.value = String(spanH);

    return { col: col - 1, row: row - 1, span_w: spanW, span_h: spanH };
  }

  function updateLayoutFromInputs(tab) {
    if (currentTileIndex === -1) return;
    const layout = normalizeLayoutInputs(tab);
    const tiles = getTilesData(tab);
    const tileEl = document.getElementById(tab + '-tile-' + currentTileIndex);
    if (tileEl && (!Array.isArray(tiles) || tiles.length === 0)) {
      setTileGridPosition(tileEl, layout.col, layout.row, layout.span_w, layout.span_h);
      return;
    }
    if (!Array.isArray(tiles) || currentTileIndex >= tiles.length) return;
    const tile = tiles[currentTileIndex] || {};
    tile.col = layout.col;
    tile.row = layout.row;
    tile.span_w = layout.span_w;
    tile.span_h = layout.span_h;
    const typeEl = document.getElementById(tab + '_tile_type');
    const typeNum = typeEl ? parseInt(typeEl.value, 10) : 0;
    tile.type = isNaN(typeNum) ? 0 : typeNum;
    tiles[currentTileIndex] = tile;
    layoutTiles(tab, tiles);
  }

  function applyLayoutInputsFromLayout(tab, layout, persistDraft = true) {
    if (!layout) return;
    const colEl = document.getElementById(tab + '_tile_col');
    const rowEl = document.getElementById(tab + '_tile_row');
    const spanWEl = document.getElementById(tab + '_tile_span_w');
    const spanHEl = document.getElementById(tab + '_tile_span_h');
    const colVal = String(layout.col + 1);
    const rowVal = String(layout.row + 1);
    if (colEl) colEl.value = colVal;
    if (rowEl) rowEl.value = rowVal;
    if (spanWEl && layout.span_w !== undefined) spanWEl.value = String(layout.span_w);
    if (spanHEl && layout.span_h !== undefined) spanHEl.value = String(layout.span_h);
    const tabDrafts = persistDraft ? drafts[tab] : null;
    if (tabDrafts && tabDrafts[currentTileIndex]) {
      tabDrafts[currentTileIndex].col = colVal;
      tabDrafts[currentTileIndex].row = rowVal;
      if (layout.span_w !== undefined) tabDrafts[currentTileIndex].span_w = String(layout.span_w);
      if (layout.span_h !== undefined) tabDrafts[currentTileIndex].span_h = String(layout.span_h);
      persistDrafts();
    }
  }
