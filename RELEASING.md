# Releasing HomeTiles

Releases are built and published automatically by GitHub Actions
([`.github/workflows/firmware.yml`](.github/workflows/firmware.yml)).
You never upload binaries by hand — you only bump the version and push a tag.

## Steps

```bash
# 1. Bump the version (only when the current state is actually ready to ship!)
#    Edit version.txt:  #define FW_VERSION "v0.6.5"

# 2. Commit, tag, push (the tag must match FW_VERSION exactly)
git add version.txt
git commit -m "Release v0.6.5"
git tag v0.6.5
git push --follow-tags
```

That's it. The action then:

1. Builds all nine release targets with the pinned toolchain (ESP32 core +
   libraries, see workflow `env`). Device names in CI identify the exact
   hardware profile; concrete validation notes belong in the release notes.
2. Verifies that the tag matches `FW_VERSION` in `version.txt` — a mismatch
   fails the build on purpose.
3. Verifies the device descriptor embedded in each binary.
4. Creates the GitHub release with auto-generated notes and uploads all
   18 binaries (`<device>.bin` for OTA + `<device>_factory.bin` for first flash).

After all 18 assets were uploaded successfully, the release job explicitly
dispatches the documentation workflow for the release tag. This explicit
`workflow_dispatch` is required because GitHub suppresses ordinary follow-up
workflow events created with `GITHUB_TOKEN`. The documentation workflow
validates the installer device/asset contract, downloads the same 18 published
release assets, verifies their GitHub SHA-256 digests, and places them in the
generated documentation site under `firmware/latest/`. Normal documentation
changes pushed to `master` still deploy through the workflow's filtered `push`
trigger. The browser installer must fetch firmware from that same HTTPS origin
because GitHub release downloads do not provide the cross-origin response
header required by browser flashing. The `gh-pages` deployment is an orphan
snapshot so successive full-size factory images do not accumulate in the
branch history.

Keep the two browser operations distinct. **Factory** must erase the full chip
and write the merged image at `0x0`. **Update** must never use that merged image
or an install flow that can opt into a full erase; it verifies `partitions.csv`
and the current redundant OTA selection, writes the regular app image once to
the inactive slot, verifies it, and only then commits a new redundant OTA
selection entry. NVS and LittleFS remain untouched.
`tools/test-installer-otadata.mjs` and `tools/test-web-installer.mjs` guard this
contract.

The checked-in release notes are not copied into the GitHub release
automatically. After the workflow succeeds, replace the generated GitHub text
with the matching `docs/releases/vX.Y.Z.md` content and keep the asset list.

The release notes must keep **hardware-confirmed** devices and devices with
**pending hardware validation** in separate sections. A successful CI compile
does not by itself prove display, touch, storage, networking, or OTA behavior.

Devices pick up the new version via their GitHub OTA check as soon as the
release is published (GitHub CDN propagation can add a few minutes).

## Rules that prevent past mistakes

- **Only the tag push triggers a release build.** Normal pushes to `master`
  build nothing (PRs do get build checks). Bumping `version.txt` alone does
  not release anything.
- **Don't bump `version.txt` while still developing.** If you flash a dev
  build that already carries the final version string, that device will later
  think it is up to date and skip the real OTA update (this happened with
  v0.3.3). Bump the version as the last step before tagging.
- **Don't pre-create a draft release for the tag.** The workflow can't see
  drafts and would create a second release. If you want custom release notes,
  edit them *after* the workflow finishes (web UI or `gh release edit`).
- **Keep target-specific networking paths separated.** All published P4
  release targets use the checked-in a8204 ESP-Hosted baseline; the ESP32-S3
  target uses native WiFi. CI verifies the expected markers before packaging.

## Preparing a candidate without releasing

Push a release-preparation branch, open a draft pull request, or dispatch the
firmware workflow manually for that branch. The workflow builds all targets and
keeps the binaries as private GitHub Actions artifacts for 14 days. Do not bump
`version.txt`, create the final tag, or create a GitHub draft release. This gives
the candidate full GitHub build coverage without making it available to OTA.

## If something goes wrong

- A failed run can simply be re-run from the Actions tab — asset upload uses
  `--clobber`, so re-runs are idempotent.
- Tag pushed but wrong/missing version bump? Fix `version.txt`, then move the
  tag: `git tag -f vX.Y.Z && git push -f origin vX.Y.Z` (the re-run rebuilds
  and re-uploads).
