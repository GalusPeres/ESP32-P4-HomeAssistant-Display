// Execute the documentation popup code with measured-style geometry and events.
// Hovering down the status column must remain possible while a note is open.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const source = fs.readFileSync(new URL('../../../docs/javascripts/device-status.js', import.meta.url), 'utf8');

function scenario(width, columnLeft, columnRight, anchorTop = 260) {
  const timers = new Map();
  let nextTimer = 0;
  const doc = { activeElement: null };
  class Element {
    constructor(rect) {
      this.rect = rect;
      this.events = {};
      this.attributes = {};
      this.style = {};
      this.open = false;
      this.hovered = false;
    }
    addEventListener(name, callback) { (this.events[name] ||= []).push(callback); }
    fire(name, event = {}) {
      for (const callback of this.events[name] || []) callback({ preventDefault() {}, ...event });
    }
    getBoundingClientRect() {
      const left = Number.parseFloat(this.style.left) || this.rect.left;
      const top = Number.parseFloat(this.style.top) || this.rect.top;
      const w = Number.parseFloat(this.style.width) || this.rect.width;
      return { left, top, right: left + w, bottom: top + this.rect.height, width: w, height: this.rect.height };
    }
    setAttribute(name, value) { this.attributes[name] = value; }
    getAttribute(name) { return this.attributes[name]; }
    matches(selector) { return selector === ':popover-open' ? this.open : this.hovered; }
    contains(element) { return element === this.child; }
    focus() { doc.activeElement = this; }
    showPopover() { this.open = true; this.fire('toggle', { newState: 'open' }); }
    hidePopover() { this.open = false; this.fire('toggle', { newState: 'closed' }); }
    querySelector() { return this.child; }
    closest() { return this.cell; }
  }
  const entries = Array.from({ length: 6 }, (_, index) => {
    const top = anchorTop + index * 40;
    const cell = new Element({ left: columnLeft, top, width: columnRight - columnLeft, height: 40 });
    const button = new Element({ left: columnLeft + 20 + index * 8, top: top + 8, width: 24, height: 24 });
    const note = new Element({ left: 0, top: 0, width: 350, height: 160 });
    button.cell = cell;
    button.attributes.popovertarget = `note-${index}`;
    note.child = new Element({ left: 0, top: 0, width: 24, height: 24 });
    return { button, note, cell };
  });
  doc.querySelectorAll = () => entries.map(entry => entry.button);
  doc.getElementById = id => entries[Number(id.split('-')[1])].note;
  vm.runInNewContext(source, {
    document: doc, innerWidth: width, innerHeight: 900,
    addEventListener() {},
    setTimeout(callback) { timers.set(++nextTimer, callback); return nextTimer; },
    clearTimeout(id) { timers.delete(id); },
  });
  const flush = () => { for (const [id, callback] of [...timers]) { timers.delete(id); callback(); } };
  return { entries, flush };
}

for (const [width, left, right] of [[1680, 1050, 1190], [1280, 850, 990], [980, 620, 760]]) {
  const { entries, flush } = scenario(width, left, right);
  for (const entry of entries) {
    entry.button.fire('pointerenter', { pointerType: 'mouse' });
    assert.equal(entry.note.open, true);
    const box = entry.note.getBoundingClientRect();
    assert.ok(box.right < left || box.left > right, `${width}px: popup covers the status column`);
    assert.ok(box.left >= 12 && box.right <= width - 12);
    assert.equal(entries.filter(other => other.note.open).length, 1);
  }
  const entry = entries.at(-1);
  entry.button.fire('pointerleave');
  entry.cell.hovered = true;
  flush();
  assert.equal(entry.note.open, true, 'Moving across the status cell must keep its links reachable');
  entry.cell.hovered = false;
  entry.cell.fire('pointerleave');
  entry.note.fire('pointerenter');
  flush();
  assert.equal(entry.note.open, true);
  entry.note.fire('pointerleave');
  flush();
  assert.equal(entry.note.open, false);
  entry.button.fire('click');
  entry.button.fire('pointerleave');
  flush();
  assert.equal(entry.note.open, true, 'Click keeps the note open');
  entry.button.fire('click');
  assert.equal(entry.note.open, false);
}

for (const top of [70, 750]) {
  const { entries } = scenario(390, 155, 315, top);
  const entry = entries[0];
  entry.button.fire('click');
  const box = entry.note.getBoundingClientRect();
  assert.ok(box.left >= 12 && box.right <= 378 && box.top >= 12 && box.bottom <= 888);
}
console.log('Documentation status notes keep the hover path clear and remain reachable.');
