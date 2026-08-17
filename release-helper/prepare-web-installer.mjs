import { createHash } from "node:crypto";
import { createWriteStream } from "node:fs";
import { mkdir, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";
import { fileURLToPath } from "node:url";

import {
  DEVICE_PROFILES,
  buildReleaseIndex,
} from "../docs/assets/javascripts/installer-contract.mjs";

const REPOSITORY = "GalusPeres/HomeTiles";
const LATEST_RELEASE_API = `https://api.github.com/repos/${REPOSITORY}/releases/latest`;

function readCliArguments(argv) {
  let outputValue = "";
  let deviceKey = "";
  for (let index = 0; index < argv.length; index += 2) {
    const option = argv[index];
    const value = argv[index + 1];
    if (!option?.startsWith("--") || !value || value.startsWith("--")) {
      throw new Error(
        "Usage: node release-helper/prepare-web-installer.mjs --output <directory> [--device <installer-key>]",
      );
    }
    if (option === "--output" && !outputValue) outputValue = value;
    else if (option === "--device" && !deviceKey) deviceKey = value;
    else throw new Error(`Unknown or duplicate option: ${option}`);
  }
  if (!outputValue) {
    throw new Error(
      "Usage: node release-helper/prepare-web-installer.mjs --output <directory> [--device <installer-key>]",
    );
  }

  const output = path.resolve(outputValue);
  const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
  const siteRoot = path.join(repositoryRoot, "site");
  const relative = path.relative(siteRoot, output);
  if (relative.startsWith("..") || path.isAbsolute(relative)) {
    throw new Error("The browser-installer output must stay inside the generated site directory.");
  }
  return { output, deviceKey };
}

function githubHeaders() {
  const headers = {
    Accept: "application/vnd.github+json",
    "User-Agent": "HomeTiles-web-installer-publisher",
    "X-GitHub-Api-Version": "2022-11-28",
  };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  return headers;
}

async function fetchLatestRelease() {
  const response = await fetch(LATEST_RELEASE_API, { headers: githubHeaders() });
  if (!response.ok) {
    throw new Error(`GitHub latest-release request failed: HTTP ${response.status}`);
  }
  return response.json();
}

async function fetchReadyLatestRelease() {
  let lastError;
  for (let attempt = 1; attempt <= 20; attempt += 1) {
    try {
      const release = await fetchLatestRelease();
      buildReleaseIndex(release, { allowMissingProfiles: true });
      return release;
    } catch (error) {
      lastError = error;
      if (attempt === 20) break;
      process.stdout.write(
        `[web-installer] Release assets are not complete yet (${error.message}); retrying in 15 seconds.\n`,
      );
      await new Promise((resolve) => setTimeout(resolve, 15000));
    }
  }
  throw lastError;
}

async function downloadAndVerify(asset, outputDirectory) {
  const response = await fetch(asset.sourceUrl, { headers: githubHeaders(), redirect: "follow" });
  if (!response.ok || !response.body) {
    throw new Error(`Download failed for ${asset.file}: HTTP ${response.status}`);
  }

  const destination = path.join(outputDirectory, asset.file);
  const temporary = `${destination}.part`;
  const hash = createHash("sha256");
  let received = 0;
  const source = Readable.fromWeb(response.body);
  source.on("data", (chunk) => {
    received += chunk.length;
    hash.update(chunk);
  });

  try {
    await pipeline(source, createWriteStream(temporary, { flags: "wx" }));
    const digest = hash.digest("hex");
    if (received !== asset.size) {
      throw new Error(`${asset.file} downloaded ${received} bytes; expected ${asset.size}.`);
    }
    if (digest !== asset.sha256) {
      throw new Error(`${asset.file} failed its GitHub SHA-256 check.`);
    }
    await rename(temporary, destination);
  } catch (error) {
    await rm(temporary, { force: true });
    throw error;
  }
}

async function downloadWithRetries(asset, outputDirectory) {
  let lastError;
  for (let attempt = 1; attempt <= 3; attempt += 1) {
    try {
      await downloadAndVerify(asset, outputDirectory);
      return;
    } catch (error) {
      lastError = error;
      if (attempt < 3) {
        process.stdout.write(
          `[web-installer] ${asset.file} failed (${error.message}); retrying.\n`,
        );
        await new Promise((resolve) => setTimeout(resolve, attempt * 3000));
      }
    }
  }
  throw lastError;
}

export function selectDevicesForPublication(index, deviceKey = "") {
  if (!deviceKey) {
    return Object.freeze({
      ...index,
      partial: index.devices.length !== DEVICE_PROFILES.length,
    });
  }
  if (!DEVICE_PROFILES.some((candidate) => candidate.key === deviceKey)) {
    throw new Error(`Unknown installer device key: ${deviceKey}`);
  }
  const device = index.devices.find((candidate) => candidate.key === deviceKey);
  if (!device) {
    throw new Error(`Release ${index.tag} has no complete firmware pair for ${deviceKey}.`);
  }
  return Object.freeze({ ...index, partial: true, devices: Object.freeze([device]) });
}

export async function prepareWebInstaller(outputDirectory, release, deviceKey = "") {
  const index = selectDevicesForPublication(
    buildReleaseIndex(release, { allowMissingProfiles: true }),
    deviceKey,
  );
  await mkdir(outputDirectory, { recursive: true });
  for (const device of index.devices) {
    for (const mode of ["update", "factory"]) {
      const asset = device[mode];
      process.stdout.write(`[web-installer] Downloading ${asset.file}\n`);
      await downloadWithRetries(asset, outputDirectory);
    }
  }

  const publicIndex = {
    ...index,
    devices: index.devices.map((device) => ({
      ...device,
      update: { file: device.update.file, size: device.update.size, sha256: device.update.sha256 },
      factory: { file: device.factory.file, size: device.factory.size, sha256: device.factory.sha256 },
    })),
  };
  await writeFile(
    path.join(outputDirectory, "release.json"),
    `${JSON.stringify(publicIndex, null, 2)}\n`,
    "utf8",
  );
  process.stdout.write(
    `[web-installer] Published same-origin assets for ${index.tag}` +
      (deviceKey
        ? ` (${index.devices[0].key} local subset)`
        : index.partial
          ? ` (${index.devices.length} release-ready devices)`
          : "") +
      "\n",
  );
}

async function main() {
  const { output, deviceKey } = readCliArguments(process.argv.slice(2));
  await prepareWebInstaller(output, await fetchReadyLatestRelease(), deviceKey);
}

if (path.resolve(process.argv[1] || "") === fileURLToPath(import.meta.url)) {
  main().catch((error) => {
    process.stderr.write(`[web-installer] ${error.message}\n`);
    process.exitCode = 1;
  });
}
