# Releasing HomeTiles

Releases are built and published automatically by GitHub Actions
([`.github/workflows/firmware.yml`](.github/workflows/firmware.yml)).
You never upload binaries by hand — you only bump the version and push a tag.

## Steps

```bash
# 1. Bump the version (only when the current state is actually ready to ship!)
#    Edit version.txt:  #define FW_VERSION "v0.3.8"

# 2. Commit, tag, push (the tag must match FW_VERSION exactly)
git add version.txt
git commit -m "Release v0.3.8"
git tag v0.3.8
git push --follow-tags
```

That's it. The action then:

1. Builds all eight release targets with the pinned toolchain (ESP32 core +
   libraries, see workflow `env`). This includes four hardware-confirmed
   profiles (covering five device products because the 86 Panel uses the 4B
   image) and four experimental profiles.
2. Verifies that the tag matches `FW_VERSION` in `version.txt` — a mismatch
   fails the build on purpose.
3. Verifies the device descriptor embedded in each binary.
4. Creates the GitHub release with auto-generated notes and uploads all
   16 binaries (`<device>.bin` for OTA + `<device>_factory.bin` for first flash).

The release notes must keep **hardware-confirmed** and **experimental /
community-testing** devices in separate sections. A successful CI compile does
not by itself promote a device to supported status.

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

## If something goes wrong

- A failed run can simply be re-run from the Actions tab — asset upload uses
  `--clobber`, so re-runs are idempotent.
- Tag pushed but wrong/missing version bump? Fix `version.txt`, then move the
  tag: `git tag -f vX.Y.Z && git push -f origin vX.Y.Z` (the re-run rebuilds
  and re-uploads).
