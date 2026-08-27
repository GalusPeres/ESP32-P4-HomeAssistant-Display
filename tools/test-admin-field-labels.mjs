// Most Web Admin field markup is generated as "<label>Text</label><input id=…>"
// with no for attribute, across fourteen tile type modules. Clicking the text
// then does nothing and a screen reader announces an unnamed field.
// associateFieldLabels() derives the association from that adjacency; this test
// pins the rules, including the cases it must leave alone.
import assert from 'node:assert/strict';

import {
  adminSource, extractFunction, inlineScriptSafe
} from './lib/admin-source.mjs';
import {runDomHarness} from './lib/headless-dom.mjs';

// The pass has to run for the initial page and for a lazily injected folder tab,
// otherwise a folder opened later keeps unnamed fields.
assert.ok(adminSource.includes('associateFieldLabels(tabEl);'),
  'A lazily injected folder fragment must get its labels associated');
assert.match(adminSource, /^\s*associateFieldLabels\(\);$/m,
  'The initial page must get its labels associated');

const productionFunctions = [
  'const LABELABLE_CONTROLS = ' +
    /const LABELABLE_CONTROLS = ([^;]+);/.exec(adminSource)[1] + ';',
  extractFunction('associateFieldLabels')
].join('\n\n');

// Every shape below is taken from the real generators: a plain label before a
// select or input, a label that wraps its own checkbox, and a label that already
// carries a for attribute.
const harness = `<!doctype html><html><body>
  <div id="fields">
    <label id="l_entity">Entity</label>
    <select id="folder0_sensor_entity"><option value="">--</option></select>

    <label id="l_unit">Unit</label>
    <input type="text" id="folder0_sensor_unit">

    <label id="l_area">Text</label>
    <textarea id="folder0_text_value"></textarea>

    <!-- A wrapping label followed by an unrelated control must not steal it. -->
    <label id="l_wrapped" class="inline-checkbox">
      <input type="checkbox" id="folder0_borders"> Borders
    </label>
    <input type="text" id="folder0_after_wrapped">

    <label id="l_existing" for="folder0_climate_entity">Climate</label>
    <select id="folder0_climate_entity"></select>

    <label id="l_hidden">Geometry</label>
    <input type="hidden" id="folder0_climate_geometry">

    <label id="l_nocontrol">Section</label>
    <div id="not_a_control"></div>

    <label id="l_noid">No id</label>
    <input type="text">

    <label id="l_taken">Second label</label>
    <select id="folder0_climate_entity_dup"></select>
    <label id="l_owner" for="folder0_climate_entity_dup">Owner</label>
  </div>
  <pre id="result">running</pre>
  <script>
  (() => {
    ${inlineScriptSafe(productionFunctions)}
    const attr = id => document.getElementById(id).getAttribute('for');
    try {
      associateFieldLabels();
      // Running twice must be idempotent.
      associateFieldLabels();

      const expected = {
        l_entity: 'folder0_sensor_entity',
        l_unit: 'folder0_sensor_unit',
        l_area: 'folder0_text_value',
        l_existing: 'folder0_climate_entity'
      };
      for (const [id, target] of Object.entries(expected)) {
        if (attr(id) !== target) {
          throw new Error(id + ' should point at ' + target +
            ' but points at ' + attr(id));
        }
      }

      // A wrapping label, a hidden input, a non-control sibling and a control
      // without an id must all stay untouched.
      for (const id of ['l_wrapped', 'l_hidden', 'l_nocontrol', 'l_noid']) {
        if (attr(id) !== null) {
          throw new Error(id + ' must stay unassociated, got ' + attr(id));
        }
      }

      const wrapped = document.getElementById('l_wrapped');
      if (wrapped.control !== document.getElementById('folder0_borders')) {
        throw new Error('A wrapping label must keep its nested control');
      }
      if (document.getElementById('folder0_after_wrapped').labels.length !== 0) {
        throw new Error('A wrapping label must not claim the next control');
      }

      // A control another label already owns must not be stolen.
      if (attr('l_taken') !== null) {
        throw new Error('An already labelled control must not be reassigned');
      }

      // The association is what makes the label clickable and the field named.
      const label = document.getElementById('l_unit');
      const input = document.getElementById('folder0_sensor_unit');
      if (label.control !== input) {
        throw new Error('The label must resolve to its control');
      }
      if (input.labels.length !== 1 || input.labels[0] !== label) {
        throw new Error('The control must report exactly its own label');
      }

      document.body.dataset.result = 'pass';
      document.getElementById('result').textContent = 'pass';
    } catch (error) {
      document.body.dataset.result = 'fail';
      document.getElementById('result').textContent = error.stack || String(error);
    }
  })();
  </script>
</body></html>`;

if (!runDomHarness({
  label: 'Admin field label association DOM test',
  html: harness,
  tmpPrefix: 'hometiles-field-labels-'
})) {
  console.log('Admin field label source contract passed.');
}
