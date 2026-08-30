// Shared headless-browser runner for the Admin DOM contract tests. Keeping the
// browser lookup and the Chrome argument list in one place stops the tests from
// drifting apart; the folder PIN test previously searched Windows paths only
// and could never run on Linux or macOS.
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {pathToFileURL} from 'node:url';

export function findBrowser() {
  const candidates = [
    process.env.CHROME_PATH,
    '/usr/bin/google-chrome',
    '/usr/bin/google-chrome-stable',
    '/usr/bin/chromium',
    '/usr/bin/chromium-browser',
    '/usr/bin/microsoft-edge',
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    '/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge',
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe'
  ].filter(Boolean);
  return candidates.find(candidate => fs.existsSync(candidate));
}

function browserIsRequired() {
  return process.env.GITHUB_ACTIONS === 'true';
}

// Renders an inline harness in headless Chrome and asserts on the dumped DOM.
// A local checkout may skip when no browser is installed, while GitHub Actions
// treats the missing browser as an infrastructure failure.
export function runDomHarness({
  label,
  html,
  tmpPrefix,
  expect = /data-result="pass"/,
  extraArgs = [],
  timeoutMs = 30000,
  browserPath = findBrowser()
}) {
  const browser = browserPath;
  if (!browser) {
    const message =
      `${label} needs a local Chrome or Edge build (set CHROME_PATH).`;
    if (browserIsRequired()) {
      throw new Error(`GitHub Actions must run this DOM test: ${message}`);
    }
    console.log(`SKIP: ${message}`);
    return false;
  }

  const temporaryDirectory = fs.mkdtempSync(path.join(os.tmpdir(), tmpPrefix));
  const harnessPath = path.join(temporaryDirectory, 'index.html');
  try {
    fs.writeFileSync(harnessPath, html, 'utf8');
    const args = [
      '--headless=new',
      '--disable-gpu',
      '--disable-extensions',
      '--no-first-run',
      '--no-default-browser-check',
      '--disable-background-networking',
      ...extraArgs,
      '--dump-dom',
      pathToFileURL(harnessPath).href
    ];
    // Chrome refuses to start its zygote sandbox as root, which is the default
    // in container builds. Only relax it for that case.
    if (typeof process.getuid === 'function' && process.getuid() === 0) {
      args.unshift('--no-sandbox');
    }
    const run = spawnSync(browser, args, {encoding: 'utf8', timeout: timeoutMs});
    assert.equal(run.error, undefined, run.error?.message);
    assert.equal(run.status, 0, run.stderr || run.stdout);
    assert.match(run.stdout, expect,
      `${label} failed:\n${run.stdout}\n${run.stderr}`);
    console.log(`${label} passed.`);
    return true;
  } finally {
    fs.rmSync(temporaryDirectory, {recursive: true, force: true});
  }
}
