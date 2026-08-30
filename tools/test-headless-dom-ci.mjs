import assert from 'node:assert/strict';

import {runDomHarness} from './lib/headless-dom.mjs';

const previousGitHubActions = process.env.GITHUB_ACTIONS;
const originalConsoleLog = console.log;

try {
  process.env.GITHUB_ACTIONS = 'true';
  assert.throws(
    () => runDomHarness({
      label: 'CI DOM contract',
      html: '',
      tmpPrefix: 'hometiles-ci-dom-',
      browserPath: null,
    }),
    /GitHub Actions must run this DOM test/,
  );

  process.env.GITHUB_ACTIONS = 'false';
  const messages = [];
  console.log = message => messages.push(String(message));
  assert.equal(runDomHarness({
    label: 'Local DOM contract',
    html: '',
    tmpPrefix: 'hometiles-local-dom-',
    browserPath: null,
  }), false);
  assert.match(messages.join('\n'), /^SKIP: Local DOM contract needs a local/);
} finally {
  console.log = originalConsoleLog;
  if (previousGitHubActions === undefined) {
    delete process.env.GITHUB_ACTIONS;
  } else {
    process.env.GITHUB_ACTIONS = previousGitHubActions;
  }
}

console.log('Headless DOM CI requirement: PASS');
