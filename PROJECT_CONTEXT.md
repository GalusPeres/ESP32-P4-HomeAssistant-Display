# HomeTiles shared project context

Last reviewed: 2026-09-03

This is the concise cross-agent project state for Codex, Claude, and other
tools. `AGENTS.md` is the only rulebook. Replace stale facts here instead of
adding model-specific handoffs or a chronological diary.

## Sources of truth

- Firmware version: `version.txt`
- Current code state: `git status`, recent commits, and the checked-out branch
- Device support and validation: the tables in `README.md` and
  `docs/flashing.md`
- ESP32-P4/ESP-Hosted patch details:
  `tools/esp-hosted-3.3.7-rx-fix/README.md`
- Release procedure: `RELEASING.md`
- Live bug status: the current GitHub issue and its newest comments; recheck
  online before changing an issue status
- HomeTiles Bridge is a separate repository. Firmware work never implicitly
  authorizes Bridge commits, pushes, or releases, and vice versa.

## Current firmware baseline

- Firmware release `v0.6.9` adds Binary Sensor and categorical Sensor history,
  Waveshare 4.3-inch support, the Waveshare 8 thin-PPA-strip guard, B4 visible
  brightness calibration, and the stabilization work committed after `v0.6.8`.
- The commits after `33b4e06` integrate PR #33 cleanup and accessibility work
  plus the isolated JC8012 V1 startup change. They did not create a new tag or
  release.
- Recent stabilization work includes S3 update/display guards, MQTT packet
  validation, duplicate Light update coalescing, bounded S3 update work, and
  incremental Weather parsing (`e3de63c`, `631de57`, `8d5771c`, `33b4e06`).
- The `v0.6.9` source passes all 53 tests. Binary/Text-State Sensor UI paths
  were exercised on Waveshare 4B, Waveshare 8-inch, and Guition ESP32-S3;
  GitHub Actions remains responsible for the complete release build matrix.
- The experimental Guition S3 XIP/`-O2` performance path was reverted in
  `5279456`. Do not reintroduce it as an assumed optimization. It increased
  risk and did not solve the measured interaction problem.

## Hardware and validation reality

- The maintainer can directly test M5Stack Tab5, Waveshare 4B, Waveshare
  8-inch, and Guition ESP32-4848S040 S3 hardware.
- Other exact revisions depend on community testers. A successful compile does
  not promote an untested revision to supported status.
- Similar P4 products share application code and sometimes base-board logic,
  but panel controller, initialization table, timing, touch controller, board
  revision, and firmware image remain exact-profile concerns.
- The current supported/pending matrix already lives in `README.md`; do not
  maintain a duplicate device plan.

## Active problem: GitHub issue #30

Issue: https://github.com/GalusPeres/HomeTiles/issues/30

- External hardware: Guition `JC8012P4A1C_I_W_Y` V1. The reporter uses Foscam
  cameras through Home Assistant's Generic Camera integration.
- The reporter also cannot install normal OTA updates; USB installation works.
- The `v0.6.8b1` and `v0.6.8b2` logs repeatedly show the same sequence: the
  stream connects and the first JPEG decodes successfully, then
  ESP-Hosted/SDIO reports CMD53 error `0x109`, wait timeout `0x107`, raw
  `0xcccccccc`, and an out-of-range RX length, followed by
  `rst:0xc (SW_CPU_RESET)`. One b2 failure began before opening the camera.
- The same SDIO failure can occur during ordinary MQTT startup without an open
  camera. Camera traffic and OTA both exercise the P4-to-C6 network transport;
  the evidence points below JPEG decoding and LVGL, but the exact electrical,
  board-revision, or driver cause is not yet proven.
- There is no normal panic core dump because the observed path ends in a
  software restart/transport recovery rather than an application exception.
- Beta b1 already contained the a8204 raw-PKT_LEN/pending-drain recovery and
  the short-tail CMD53 marker. It still failed, so repeating that patch or
  merely changing its label is not a solution.
- The ESP-Hosted version RPC timeout `0x15e` also occurs on a stable Waveshare
  8-inch unit. It identifies an older C6 protocol but is not sufficient to
  cause the crash; the fatal Guition-only evidence is the `0x109`/`0x107`
  transport cascade.
- Do not use or publish `v0.6.8b3`; it only requested 4-bit SDIO at 20 MHz and
  did not contain the later first-fault diagnostics. Espressif issue #167 also
  reports that 20 MHz did not reliably prevent this failure sequence.
- The b4 logs locate the first failure at a large C6-to-P4 CMD53 block read:
  raw DCRC status `0x80` occurs on 11- or 14-block reads before `0x109` and the
  later `0x107` timeout. JPEG decode, display, and Home Assistant are downstream
  of the failing transport operation.
- The b5 1-bit/40-MHz build still produces the same first DCRC failure, including
  failures before opening the camera. Reducing the active SDIO data lanes is
  therefore not a fix.
- Exact schematics explain why this Guition can behave differently from other
  P4 boards despite sharing the same P4/C6 architecture: Guition V1 uses
  5.1-kohm pull-ups on CMD, CLK, and D0-D3 without series termination;
  Waveshare 8-inch uses 51-kohm pull-ups, while Tab5 combines 5.1-kohm
  pull-ups with 22-ohm series resistors and a separately switched WLAN rail.
  This makes Guition-specific SDIO signal or power margin plausible. The b6
  result proves that limiting the RX CMD53 transaction length is an effective
  mitigation, but it does not by itself prove the underlying electrical cause.
- Guition's original `JC8012P4A1_C6.bin` and the current HomeTiles host both
  use ESP-Hosted streaming mode. The newer official
  `JC-C6-slave_v2.3.2.bin` uses packet mode and is not a drop-in C6 update for
  the current host. Do not flash it alone. The onboard USB paths reach only
  the P4; C6 UART/boot access requires the internal CN5 header and an external
  3.3 V UART adapter.
- Do not use the reported ESP-Hosted 2.9.3 rollback as the next test. That
  report concerns a different `0x102` TX/alignment failure on another board;
  the normal 2.9.3-to-2.11.6 CMD53 RX path does not explain this Guition's
  `0x109` CRC failure, and rollback would discard relevant safety fixes.
- `v0.6.8b6` passed the reporter's hardware test: both cameras ran at 15-20 FPS,
  Web Admin OTA succeeded, and the previous `0x109`/`0x107` failure did not
  recur. The regular-source integration retains the proven 1-bit/40-MHz V1
  configuration and splits large P4 RX reads into individual 512-byte CMD53
  reads. It remains limited to the exact V1 profile; v0.6.9 does not contain it.
- Lowering camera FPS, resolution, or quality may be used only as a clearly
  identified diagnostic A/B test. It is not an acceptable final fix and must
  not silently reduce normal camera performance.
- Keep the release integration limited to the exact V1 profile and verify the
  single-block marker plus 1-bit configuration in its final build. Other P4
  profiles must retain their baseline ESP-Hosted object; S3 remains unaffected.

## ESP32-P4 network history that remains relevant

- The repository already backports ESP-Hosted allocation/PSRAM fixes,
  synchronous RPC UID routing, Espressif's `a8204f9` dropped-RX recovery, and
  sparse diagnostics. Exact patches, variants, hashes, and limitations are in
  `tools/esp-hosted-3.3.7-rx-fix/README.md`; do not duplicate them here.
- The `repo-a8204` variant is the release-safe baseline. The short-tail receive
  variant was an experimental field path and is not proof of a universal fix.
- Historical P4 OTA experiments showed that generic transfer throttling,
  PSRAM-only staging, direct TLS-to-flash streaming, in-place ESP-Hosted
  restart, and extra permanent SDIO buffers did not cure the underlying
  failure. Do not repeat them without new evidence and an isolated test.
- Existing safeguards against a permanent network wedge are recovery, not
  proof that the transport defect is solved.

## Binary and textual Sensor history in v0.6.9

- Firmware adds stable tile type 20, reusing `sensor_entity`
  without changing `PackedTileV7`.
- Device UI/Web Admin use central DE/EN/FR strings, state-aware HA icons,
  autosave/previews, and a responsive 24-hour/7-day Sensor popup; textual
  `sensor.*` states reuse its timeline/Activity while numbers keep the graph.
- Missing, `unknown`, and `unavailable` remain distinct; state payloads and UI
  queues are bounded. Notifications and sounds remain deferred.
- Bridge v0.6.40 (`581150b`) is released with complete bounded Recorder paging,
  categorical Sensor history, and explicit legacy-firmware compatibility.

## Product direction still requested

- Stability and exact-device validation come before broad new features.
- Later work includes notifications/sounds, event-driven folder colors, Number,
  Date/Time, Select, Button, and related entities; track priority in live issues.

## Maintenance rule for this file

- Update the review date and affected facts after a meaningful change.
- Remove resolved tasks and obsolete experiments; keep only conclusions that
  prevent repeated work.
- Link to canonical technical documents instead of copying them.
- Never paste raw serial logs, complete issue threads, temporary BIN lists, or
  chat summaries here.
