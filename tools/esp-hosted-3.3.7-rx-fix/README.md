# ESP-Hosted 3.3.7 RX/PSRAM fixes + SDIO deadlock recovery

These RISC-V objects are rebuilt from the exact ESP-Hosted release the
ESP32-P4 Arduino core 3.3.7 libraries ship (`espressif__esp_hosted 2.11.6`,
esp-hosted-mcu commit `74f5e7d64ae97a4957a1c5c88bcd23c9447ccdfa`) with
the relevant changes from Espressif's upstream fix commits applied:

- `a914274d10e8f0e754b5902c7867ff96cea4a8b9` ("fix(sdio): gracefully
  drop packets on RX mempool exhaustion")
- `971cf0d859073c4839d3b7b87bdae4c68240883c` ("fix(sdio): throttle RX
  mempool drop log to burst start/recovery")
- `d7ebe00a22e618e47a2b89f7569d5ed37163fa1e` ("fix: buffer allocation
  improvements from the audit")
- `da48eef4cb653d9eb2070644d2ea7c9a90d3fa9f` (ESP-Hosted 2.12.8,
  `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM`), backported to the P4-only
  Arduino 3.3.7 objects.
- `a8204f950927c17ecf6d737927cf34b4200ffbd3` (ESP-Hosted 2.12.12,
  "fix(sdio): recover a dropped RX read instead of deadlocking"), backported
  without the unrelated example/RPC changes from that aggregate commit.

- `issue-167-sdio-workarounds.patch`: experimental backport of the two
  ESP32-P4/ESP32-C6 workarounds reported in upstream issue #167. The
  `TOKEN_RDATA` counter is read as four CMD52 single-byte transactions
  instead of one CMD53 byte-mode transaction, and host-to-C6 transfers use
  exact-length CMD53 byte mode instead of padded block-only writes. These
  changes avoid the two C6 SLC transaction shapes reported to wedge under
  sustained traffic. They are not yet an official Espressif fix.
- `rx-short-tail-cmd53.patch`: targeted 8-inch field A/B for the remaining
  receive-side transaction shape after a long camera run ended with
  `packet size[1502]>[86]`. Host RX no longer rounds the whole stream delta
  up to a 512-byte CMD53 block transfer. Full blocks are followed by a short
  byte-mode tail, retaining the ESP port's required four-byte alignment.
  The double buffer explicitly reserves those zero-to-three alignment bytes,
  and corrupt deltas that would overflow the port's 16-bit transfer length
  fail as a transport error. This deliberately does not add parser
  reassembly, so the first field test can attribute the result to the RX
  transaction change.
- `pkt-len-pending-rx-recovery.patch`: checks the complete raw 32-bit
  `PKT_LEN` word for a true `0xFFFFFFFF` bus fault before applying the 20-bit
  counter mask. The legal low-20-bit value `0xFFFFF` is no longer discarded.
  The RX task also drains pending data and retries allocation without waiting
  for an interrupt edge which it has already acknowledged.
- `sdio_drv.c`: `sdio_rx_get_buffer()` returns NULL instead of `assert(*buf)`
  on allocation failure and now retries that same pending read after 1 ms.
  Field crash fixed: `assert failed: sdio_rx_get_buffer sdio_drv.c:896 (*buf)`
  (waveshare_touch_lcd_8, v0.4.14, task `sdio_read`).
  Streaming RX now skips only the packet whose `pkt_rxbuff` allocation failed
  instead of asserting, and logs one OOM-start/OOM-end pair per exhaustion
  burst. Field crash fixed: `assert failed: sdio_push_data_to_queue
  sdio_drv.c:964 (pkt_rxbuff)` (waveshare_touch_lcd_8, v0.5.6, tasks
  `sdio_rx_buf` and `loopTask`). The shared SDIO TX mempool allocation also
  fails gracefully instead of asserting.
- `port_esp_hosted_host_os.c`: `hosted_realloc()` uses libc `realloc` instead
  of malloc + memcpy(newsize), which over-read the old block by the growth
  delta (heap over-read; the serial RX reassembly buffer grows through it).
  Aligned ESP-Hosted transport/mempool allocations now try DMA-capable PSRAM
  first and retain the original internal-DMA allocation as a fallback. This is
  Espressif's upstream P4 solution for memory-intensive sustained video and
  keeps the internal DMA heap available for lwIP, MQTT, and the display.

Build verification: a rebuild containing only the previous HomeTiles
backports matches the previous repository object opcode-for-opcode
(remaining diffs are path-string offsets only) and has an identical symbol
table. The issue #167 build then changes only `sdio_drv.c`: the four CMD52
reads are compiled into `sdio_write_task`, and the block-padding branch is
compiled out while the RX block-transfer setting remains unchanged. The
subsequent short-tail A/B object also compiles out RX 512-byte padding, keeps
the four-byte port alignment, and contains the overflow guard and a distinct
startup marker.
The patched objects remove four fatal allocation asserts from the affected
SDIO RX/TX paths, add `realloc` as an undefined symbol (resolved by newlib/ESP
heap), and call `heap_caps_aligned_alloc()` first with
`MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT`, then with the original
internal-DMA capabilities if PSRAM allocation fails. The a8204 backport adds
no new undefined symbol; normal and `-Wall -Wextra` builds are byte-identical.

The field diagnostics deliberately use sparse ERROR-level markers because
Arduino's precompiled ESP-Hosted component removes lower log levels. Startup
prints `HomeTiles SDIO RX recovery active` and
`HomeTiles SDIO RX 512-byte padding disabled`; legal `0xFFFFF`, pending-drain,
allocation-retry, and true raw-bus-fault counters are logged for the first
four hits and then at powers of two. `hometiles_sdio_get_rx_diag()` exposes a
lock-free snapshot to the persistent HomeTiles network-wedge report. It never
takes the possibly stalled SDIO mutex.

Toolchain: crosstool-NG esp-14.2.0_20251107 (riscv32-esp-elf-gcc 14.2.0),
compiled with each variant's shipped `flags/` + `qio_qspi/include/sdkconfig.h`
plus `-Os`. The firmware workflow injects the objects into each precompiled
core archive and verifies the result before compiling. Absolute compile paths
remain in DWARF/`__FILE__`, so the full object SHA below identifies these exact
artifacts; opcode/symbol verification is used for rebuild comparison.

## Per-device release variants

The CMD53 short-tail change is still an 8-inch field experiment and must not be
silently promoted to every ESP32-P4 image:

- `repo-short-tail` uses the fully patched `sdio_drv.c.obj` below and is built
  only as a clearly named, non-release Waveshare 8-inch A/B artifact until a
  24-48 hour camera soak has passed without a wedge.
- `repo-a8204` uses the checked-in `baseline-a8204` SDIO object while retaining
  the common RPC, allocation, PSRAM, PKT_LEN, and pending-drain fixes. It is the
  published release variant for every ESP32-P4 target.
- The ESP32-S3 target uses native WiFi and links no ESP-Hosted object.

The local build helper defaults to the release-safe a8204 variant; a short-tail
test build must select `-EspHostedRxVariant repo-short-tail` explicitly. GitHub
Actions builds both 8-inch variants under distinct artifact names and verifies
their final binary markers. The baseline object hashes are:

- `baseline-a8204/esp32p4-libs/sdio_drv.c.obj`
  - SHA-256: `544aa1cb70ed77dad73eff11efc2d3faa1ccc9cda64dcf291bcd55aa50a3764f`
- `baseline-a8204/esp32p4_es-libs/sdio_drv.c.obj`
  - SHA-256: `0549005b2e710f3ac98795049f4a1a1cb8c74a070c7801d66039be13144f1c53`

- `pkt-len-pending-rx-recovery.patch`
  - SHA-256: `bfcb601c22c56db2768b2c90dd0fb38c38f1a766804156e3e1c8b29df8a8bae0`
- `rx-short-tail-cmd53.patch`
  - SHA-256: `094d919d45d37ea2e280f0164dc8558417fdfbb27d4c414c87e91dd892cbeba2`
- fully patched `sdio_drv.c` source
  - SHA-256: `bd385c4aa4c88d4e05a4433e1618ad121dd1bb967f9e3afb5e4bf73156621617`

- `esp32p4-libs/sdio_drv.c.obj`
  - SHA-256: `bd37eac387ec44c1f124304108136fb33765f8b6da39ba4c51d31f99a5684a6f`
- `esp32p4-libs/port_esp_hosted_host_os.c.obj`
  - SHA-256: `f82f73b3af661fec2b5908924349c0f2db1d557cb2a959b7db4d6927ef374828`
- `esp32p4_es-libs/sdio_drv.c.obj`
  - SHA-256: `ca63bcf8c5da34d14f65bc4f729cc55b49b38c60cf6493c03b9ee41eb3da6cab`
- `esp32p4_es-libs/port_esp_hosted_host_os.c.obj`
  - SHA-256: `d4d88a04edcd6f7de78f63a75df0d296ba0531ffc3182e73795ce3a687043ab4`

## RPC synchronization and UID routing

`rpc-sync-serialization.patch` fixes a separate ESP-Hosted 2.11.6 RPC
deadlock seen during concurrent P4/C6 WiFi state changes. The shipped core
posts a per-UID semaphore after placing every synchronous response into one
global three-entry FIFO. Concurrent waiters can therefore consume each
other's response. A response arriving after its waiter timed out remains in
that FIFO; three stale responses fill it, and the next blocking enqueue can
wedge the sole RPC RX task.

The backport serializes complete synchronous transactions and stores each
response directly in its UID slot. It also:

- allocates UIDs and changes response-table ownership under a short mutex;
- drops orphaned, duplicate, and mismatched responses instead of queueing
  them;
- linearizes response delivery against sync and async timeout cleanup;
- keeps request buffers owned until TX has consumed their external pointers;
- atomically detaches request-owned handles before freeing them so a hard
  FreeRTOS task cancel cannot turn recovery cleanup into a double-free;
- rejects new sends during deinit and retains synchronization mutexes across
  deinit/reinit so blocked callers cannot use destroyed locks;
- snapshots event callbacks atomically across unregister/deinit races; and
- avoids dereferencing a request in `rpc_slave_if.c` after a failed send has
  released it.

The synchronous transaction lock is intentionally persistent. The core logs
`HomeTiles RPC sync serialization active (UID-routed responses)` during init;
the firmware build also requires this marker before accepting a P4 build.

Both objects were compiled from the exact 2.11.6 source baseline with each
Arduino 3.3.7 P4 variant's shipped flags and also compile cleanly with
`-Wall -Wextra`:

- `rpc-sync-serialization.patch`
  - SHA-256: `2dbd973415636fadee86285a3327756ba9c5cba8f33617d5fc9d0de1e8e798c3`
- patched `rpc_core.c` source
  - SHA-256: `ecf4860de2db542a5a985f76f5378c62ac4e95898649fa1cf9da9f7faf19c2aa`
- patched `rpc_slave_if.c` source
  - SHA-256: `f0fc68043b1a514d28d8696a9dc6c42768bad4abaa872b2f353290efc7a8088f`

- `esp32p4-libs/rpc_core.c.obj`
  - SHA-256: `3c94542e9e7a85ea079cbcc473a3542c3ba7f816da63543ccd5d5ab992f124a6`
- `esp32p4-libs/rpc_slave_if.c.obj`
  - SHA-256: `66c89f37c2235667b65be6b52b7dd75a86c2c26d55e9a94ef5c71dbbf9ce0cb5`
- `esp32p4_es-libs/rpc_core.c.obj`
  - SHA-256: `3eda43c1efc997d640dd219096ebb04cd6075039678bbfd27dee203787571f80`
- `esp32p4_es-libs/rpc_slave_if.c.obj`
  - SHA-256: `703bf28301ec771bff0a6451b60cbb20cf4279703b027cc54bf5186661d65b9a`
