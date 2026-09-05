# HomeTiles architecture and maintenance guide

This contributor guide explains ownership boundaries and remaining maintenance
work. See [AGENTS.md](AGENTS.md) for rules, [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md)
for current validation and [RELEASING.md](RELEASING.md) for build/release procedures.

The priority is stable, responsive firmware; refactoring should clarify ownership and make behavior easier to test or extend.

## Subsystem map

| Area | Primary implementation | Responsibility and boundary |
| --- | --- | --- |
| Application orchestration | `HomeTiles.ino` | Startup, operating modes, pending actions, service ordering, LVGL cadence |
| Hardware | `src/devices/`, `src/core/board_hal.*` | Exact-board initialization, display/touch/storage capabilities and hardware access |
| Display and power | `src/core/display_manager.*`, `power_manager.*` | LVGL buffers, input, brightness, refresh cadence, sleep/wake sequencing |
| Device settings | `src/core/config_manager.*` | Settings normalization, legacy migration, NVS persistence and live configuration |
| Settings access record | `src/core/settings_access_record.h` | Packed parental-control record encoding, validation and version migration |
| Tile persistence | `src/tiles/tile_config.*` | Stable packed tiles, folder hierarchy, sidecars and filesystem recovery |
| Tile type contract | `src/tiles/tile_type*.h`, `src/types/types_registry.*` | Persisted IDs, eligibility policies and type descriptors |
| Tile presentation | `src/types/<type>/`, `src/tiles/tile_renderer.*` | Type renderers, widget bindings, queued state application and shared rendering helpers |
| Folder presentation | `src/ui/tab_tiles_unified.*` | Active grid, cached folders, navigation, reloads and cached-state dispatch |
| Popups | `src/ui/*_popup.*`, `popup_layout.h` | Reusable UI objects, interactions, pending responses and local lifecycle |
| Networking | `src/network/network_transport.*`, `network_manager.*` | Network selection, recovery, MQTT socket ownership and outbound scheduling |
| Bridge integration | `src/network/mqtt_handlers.*`, `ha_bridge_config.*` | Topic routing, entity metadata, state caches and application commands |
| Camera | `src/video/camera_stream.*`, `src/ui/camera_popup.*` | Network/JPEG worker, frame ownership and loop-side presentation |
| Web Admin | `src/web/`, `src/web/admin/`, `src/types/*/admin*.js` | HTTP configuration interface, browser editor, previews and autosave |
| Firmware update | `src/core/github_update.*`, Web Admin OTA handlers | Device validation, transport, staging, flash writes and update recovery |
| Localization | `src/core/i18n.*` | Shared device/Web Admin strings and language-dependent formatting |
| Build metadata | `tools/device-profiles.json`, `tools/device-catalog.js` | Build, release, silicon and installer identity outside the firmware runtime |

Dependencies are not yet one-way: rendering reads configuration/Bridge metadata,
network recovery knows camera activity, and popup opening knows peer popups.

## Source size versus responsibility

Generated fonts and compressed Web assets, icon mappings, logo pixel data and
vendor touch-controller firmware tables contain substantial compiled data.
Splitting that data does not simplify application behavior. Handwritten hotspots
include `tile_renderer.cpp`, `tile_config.cpp`, `tab_tiles_unified.cpp`,
`mqtt_handlers.cpp`, the large popups and HTTP handlers. Their parsing, storage,
networking and presentation responsibilities are the useful extraction targets.

## Data flow and thread ownership

The Arduino loop owns LVGL and normal application state mutation. It handles
deferred HTTP/UI actions, consumes inbound MQTT, updates caches and widgets,
and services `lv_timer_handler()`. Active, sleep, AP and OTA modes deliberately
have different service policies.

The MQTT worker owns the MQTT client and socket. Its receive callback validates
and copies messages into a bounded queue of pointers, preferring PSRAM for
payload storage. The loop drains that queue and runs application topic handlers.
Moving socket work off the loop does not authorize moving LVGL or flash mutation
onto that worker.

The normal state path is:

```text
Home Assistant / Bridge -> MQTT worker -> inbound queue -> loop topic handler
  -> entity metadata/state cache -> type update queue -> widget state -> LVGL
```

Device interactions and browser changes take different paths:

```text
Device control -> validated command -> outbound queue -> MQTT worker -> Bridge
Browser editor -> draft/autosave -> HTTP handler -> persistence -> pending reload
  -> loop grid/cache refresh -> device rendering and browser preview
```

Outbound MQTT has separate control, normal-publish and large-publish lanes.
Startup bursts, buffer growth and P4 DMA headroom affect their scheduling.
Recovery waits for camera socket ownership to end before tearing down transport.
These restrictions are stability mechanisms, not general-purpose task scheduling.

The media artwork worker owns HTTP download buffers and unpublished results.
The loop attaches image descriptors to widgets and performs LVGL cache cleanup.
Freeing an unpublished descriptor differs from releasing an image already used
by LVGL; an extraction must preserve that distinction.

Camera buffers have protected `Free`, `Writing`, `Ready`, `Presenting` and
`Displayed` states. The worker cannot overwrite a displayed/presenting buffer.
Closing a camera removes LVGL's image reference before requesting worker cleanup;
draw-buffer restoration is deferred outside the LVGL callback.

## Changes implemented in this refactoring

### One device catalog for host tooling

`tools/device-profiles.json` defines build names, device keys, exact silicon
ranges, transport variants, linker requirements and release/installer metadata.
`tools/device-catalog.js` validates those relationships before exposing them to
the local builder, importer and packager.

`tools/generate-device-profiles.mjs` generates the marked CI matrix and
`docs/assets/javascripts/device-profiles.mjs`. The installer consumes that output.
`release-helper/firmware-metadata.js` supplies shared image identity validation
for host-side packaging and size comparison.

This catalog does not replace hardware drivers or infer pins and panel timings.
`sketch.yaml`, compile-time device selection, board metadata and exact-profile
implementation remain firmware concerns. Integration tests check their agreement.

### Browser source units with unchanged deployed bytes

`src/web/admin/bundle.json` lists 51 ordered source units. General editor code
lives under `src/web/admin/`; type-specific behavior lives beside its type under
`src/types/<type>/admin*.js`.

`tools/lib/admin-bundle.mjs` assembles these into the classic script at
`src/web/assets/admin.js`. It checks source coverage, duplicate/invalid paths,
and the syntax of each unit and the assembled result. There is no browser module
loader, additional network request or runtime dependency introduced by this split.

The extraction preserves the normalized script bytes and compressed JavaScript
asset bytes from the preceding implementation. `tools/generate-web-assets.mjs`
updates generated files only when their content changes, preserving build caches.
The shared browser scope and load order remain dependencies between source units.

### Tile identities, policies and state dispatch

Each type owns its runtime records in `state.h` or `widgets.h`.
`tile_renderer.h` preserves compatibility and owns the aggregate grid cache/API.
`src/tiles/tile_type.h` owns stable numeric IDs, including retired values.
`tile_type_policy.h` separates entity persistence, cached state, popup-mode,
MQTT routing/subscription, icon-refresh and screensaver eligibility decisions.
They intentionally differ: Camera stores an entity without consuming ordinary
cached tile state, and Weather uses its own subscription path.

`tile_update_service.h` owns the ordered live-state service list. Active budgets
remain Sensor 6, Switch 6, Climate 4, Cover 4, Binary Sensor 4, Weather 4 and
Media 2. Sleep passes zero to drain each queue. Compile-time selection preserves
direct calls without a permanent callback table or runtime registration.

`enqueue_cached_tile_state()` in `tab_tiles_unified.cpp` is shared by full-grid
and single-slot restoration. It retains existing payload fallbacks, sensor unit
resolution and the ability to omit media while building hidden folder caches.
Cache construction, lifecycle resets and type-specific queues are still separate.

### Settings access codec

`settings_access_record.h` separates record encoding/validation from NVS I/O.
The implementation stays local to the calling translation unit. Legacy, v3 and
v4 records remain 60, 132 and 144 bytes with unchanged checksum coverage.
The hash remains authoritative when validating the bounded recovery PIN copy.

Corrupt parental-control metadata deliberately recovers by revealing and
unlocking Settings. That existing recovery behavior is preserved, not newly
introduced. Record rejection itself leaves the destination unchanged;
ConfigManager owns the recovery decision and subsequent normalization.

### HTTP endpoint decomposition

`web_admin_handlers.cpp` retains settings/MQTT/Bridge, status, restart and border
handlers. Endpoint families now live in `web_admin_tiles.cpp`,
`web_admin_files.cpp`, `web_admin_diagnostics.cpp`, `web_admin_screensaver.cpp`
and the existing `web_admin_hardware_io.cpp`. OTA handlers and their state now
live together in `web_admin_ota.cpp`.
`web_admin.cpp` retains server lifecycle and route registration. Small shared
helpers live in `web_admin_handler_utils.h` and `web_admin_tile_helpers.h`.
Request fields, response behavior and persistence ordering remain the contract.

## Extending the project

A new device starts with exact hardware evidence and an isolated device profile.
Add its host-tool identity to the catalog, retain exact silicon/transport guards,
then validate generated outputs, source selection and image metadata together.
Shared panel logic is appropriate only where the hardware contract truly matches.

A new tile starts with a persisted ID, explicit policies and a type descriptor.
Its module owns rendering, HTTP field handling and type-specific browser code;
central localization remains shared. Persistence, queue service, hidden caches,
screensaver eligibility and Bridge subscriptions are separate integration points.
The complete end-to-end checklist remains in AGENTS.md; the new helpers reduce
duplication but do not replace that contract.

## Audited risks and unresolved behavior

| Finding | Evidence and practical impact | Status |
| --- | --- | --- |
| P4 settings write errors can be missed | Non-S3 `BatchedNvsWrite::finish()` returns true; ConfigManager ignores most individual write results. An earlier failed key write can coexist with a reported successful save and updated RAM state. | Verified unchecked error path; fault-injection reproduction and fix pending |
| GitHub OTA does not authenticate TLS certificates | `fetchHttpRange()` uses `setInsecure()`. Device/project/silicon metadata checks image compatibility, not the identity of the download server. | Verified configuration; trust model and size-aware hardening pending |
| Queued state can outlive slot bindings | Switch/Binary Sensor guard stale layouts; several other queues retain only grid/slot targets across reset paths. | Inconsistent protection confirmed; failing navigation/rebuild scenario not yet reproduced |
| Idle backlog drains slowly | Active idle service resets its timestamp after one bounded batch every two seconds. A 31-entry sensor backlog needs six service windows, up to roughly 12 seconds. | Scheduling behavior confirmed; latency impact depends on traffic and interaction |
| Image preview work runs within LVGL service | The preview timer calls `process_preview_step()` from its callback. Work is stepped, but individual storage/decode duration still matters. | Call path confirmed; target-device timing required |
| Popup exclusion is distributed | Light, Sensor, Climate and Cover maintain different lists of peer hide calls. | Extension risk; no new overlap failure reproduced in this review |

These findings are not fixed by structural changes. Existing media deletion and
camera buffer protections remain necessary; removing them would undo prior fixes.

## Prioritized next work

1. Reproduce persistence failures and stale-slot delivery with injected errors and
   controlled queue/reset sequences. Fix proven failures with focused tests before
   changing throughput or adding more stateful tile types.
2. Separate media artwork decoding, transport and presentation. Keep widget/cache
   ownership explicit; first preserve the translation unit, then measure any
   proposed separate compilation boundary against firmware size and hot paths.
3. Separate TileConfig's record codecs from filesystem replacement/recovery and
   folder policy. Test interrupted writes, backup promotion and sidecar recovery
   before changing transaction sequencing or persistence layout.
4. Centralize popup exclusion and document each popup's hide/delete semantics.
   Final slider delivery, timer cancellation and entity rebinding remain local
   responsibilities; a shared close function must not silently discard commands.
5. Give browser editor state explicit interfaces after the byte-preserving split.
   Start with draft/snapshot/autosave boundaries and run the real preview branches.
6. Separate MQTT session/outbound scheduling from network-interface recovery only
   after worker/loop ownership is executable in tests. Preserve profile-specific
   P4 transport safeguards rather than making them generic defaults.

## Validation and performance acceptance

Host tests establish contracts, not display or network behavior on real hardware.
The codec fixtures execute production C++ with platform shims; queue and cached
state tests execute production dispatch with controlled endpoints. Browser tests
exercise editor behavior; asset checks establish deterministic generated bytes.

For this refactoring, both test-device application BINs must be no larger than
their baseline: `waveshare_8` and `guition_esp32_4848s040`. Compare matching source
baselines with the same toolchain, profile, optimization and diagnostic flags.
`tools/compare-firmware-size.mjs` checks image identity and reports byte counts,
SHA256 hashes and deltas. Padded factory images are not a useful size metric.
Final measurements belong in the generated verification report, not an assumed
claim in this guide. A smaller BIN alone does not establish better runtime speed.

| Scenario | Waveshare 8-inch P4 | Guition ESP32-4848S040 S3 | Evidence to collect |
| --- | --- | --- | --- |
| Boot and retained MQTT burst | Required | Required | Time to usable UI, missing states, queue drops, minimum internal heap |
| Folder navigation and cached restore | Required | Required | Correct entity/state binding, tap latency, loop gaps, repeated-cycle memory |
| Sensor/Binary Sensor history and unavailable states | Required | Required | Correct localized state, fresh history, missing/zero distinction |
| Light/Climate/Cover interactions | Required | Required | Drag responsiveness, final command, remote echo behavior, repeated open/close |
| Media artwork and rapid track changes | Required | Required | Decode/presentation time, late-result handling, internal/PSRAM headroom |
| Camera open/close and network recovery | Required | Unsupported | FPS, presentation timing, restored UI buffers, no transport teardown race |
| Sleep/wake and screensaver | Required | Required | Current state on wake, touch behavior, brightness and cache correctness |
| Web Admin save/reload and OTA | Required | Required | Persistence after reboot, responsive editing, correct-profile update handling |

Use existing `[LoopGap]`, media/camera timing output and optional Guition
diagnostics for baseline/candidate comparisons. Use the same configuration,
entities, payload sequence and warm-up period. Separate first-use work from warm
cache behavior, repeat interactions, and record worst gaps as well as averages.
Additional instrumentation belongs in an isolated diagnostic build; production
performance work should not add permanent buffers or per-frame logging.
