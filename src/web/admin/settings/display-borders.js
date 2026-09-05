
  let normalTileBordersSaveSequence = 0;
  function applyNormalTileBordersPreview(enabled) {
    document.querySelectorAll('.normal-tile-border-toggle').forEach(input => {
      input.checked = !!enabled;
    });
    document.querySelectorAll('.tile-grid:not(.screensaver-tile-grid)').forEach(grid => {
      grid.classList.toggle('tiles-bordered', !!enabled);
    });
  }
  async function saveNormalTileBorders(enabled) {
    const wanted = !!enabled;
    const previous = !wanted;
    const sequence = ++normalTileBordersSaveSequence;
    applyNormalTileBordersPreview(wanted);
    try {
      const response = await fetch('/api/display/tile-borders', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'enabled=' + (wanted ? '1' : '0')
      });
      if (!response.ok) throw new Error('HTTP ' + response.status);
    } catch (error) {
      if (sequence !== normalTileBordersSaveSequence) return;
      applyNormalTileBordersPreview(previous);
      showNotification(t('networkErrorSave'), false);
    }
  }
