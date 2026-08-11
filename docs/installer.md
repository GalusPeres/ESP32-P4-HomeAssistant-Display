# Flashing the Firmware

## Browser installer

<div class="ht-installer" data-hometiles-installer>
  <div class="ht-installer-form">
    <div class="ht-installer-section">
      <label class="ht-installer-section-heading" for="installer-firmware-source">1. Firmware</label>
      <div class="ht-installer-section-body">
        <p class="ht-installer-section-info">Choose the published release or a local HomeTiles .bin file.</p>
        <div class="ht-installer-control ht-installer-firmware-control">
          <select id="installer-firmware-source" autocomplete="off" disabled>
            <option value="published">Loading published release...</option>
            <option value="local">Local HomeTiles .bin file</option>
          </select>
          <input id="installer-firmware-file" class="ht-installer-firmware-file" type="file" accept=".bin,application/octet-stream" hidden>
        </div>
      </div>
    </div>

    <div class="ht-installer-section">
      <label id="installer-device-label" class="ht-installer-section-heading" for="installer-device">2. Device</label>
      <div class="ht-installer-section-body">
        <p class="ht-installer-section-info">Select the exact model printed on the device or rear label.</p>
        <div class="ht-installer-control">
          <select id="installer-device" autocomplete="off">
            <option value="">Loading devices...</option>
          </select>
          <div id="installer-device-details" class="ht-installer-device-details" data-kind="info" hidden></div>
        </div>
      </div>
    </div>

    <div class="ht-installer-section">
      <div id="installer-mode-label" class="ht-installer-section-heading">3. Flash mode</div>
      <div class="ht-installer-section-body">
        <p class="ht-installer-section-info">Update keeps settings. First install / factory reset erases all local data.</p>
        <div class="ht-installer-modes" role="radiogroup" aria-labelledby="installer-mode-label">
          <label class="ht-installer-mode">
            <input type="radio" name="installer-mode" value="update" checked>
            <strong>Update</strong>
          </label>
          <label class="ht-installer-mode ht-installer-mode-danger">
            <input type="radio" name="installer-mode" value="factory">
            <strong>First install / factory reset</strong>
          </label>
        </div>
      </div>
    </div>

    <div class="ht-installer-section">
      <div id="installer-confirm-label" class="ht-installer-section-heading">4. Confirm</div>
      <div class="ht-installer-section-body">
        <p class="ht-installer-section-info">Confirm the exact label before connecting the serial port.</p>
        <div class="ht-installer-confirmations">
          <label>
            <input id="installer-exact-hardware" type="checkbox">
            <span id="installer-exact-hardware-text">I checked the exact model label.</span>
          </label>
          <label id="installer-factory-confirmation-row" class="ht-installer-danger-check" hidden>
            <input id="installer-factory-confirmation" type="checkbox">
            <span>Erase all settings and local data.</span>
          </label>
        </div>
      </div>
    </div>

    <div class="ht-installer-section ht-installer-section-connect">
      <div class="ht-installer-section-heading">5. Connect and flash</div>
      <div class="ht-installer-section-body ht-installer-action">
        <p class="ht-installer-section-info">Keep power, USB, and this page connected until flashing finishes.</p>
        <div class="ht-installer-action-row">
          <button id="installer-flash" type="button" disabled>Connect and flash</button>
          <div id="installer-status" class="ht-installer-status" role="status" aria-live="polite" data-kind="info">Loading...</div>
        </div>
        <div id="installer-progress-panel" class="ht-installer-progress" hidden>
          <div class="ht-installer-progress-meta">
            <strong id="installer-phase">Preparing</strong>
            <span id="installer-progress-text">0%</span>
          </div>
          <progress id="installer-progress" max="100" value="0" aria-labelledby="installer-phase installer-progress-text"></progress>
        </div>
      </div>
      <div id="installer-log-panel" class="ht-installer-log-panel" hidden>
        <div class="ht-installer-log-header">
          <strong>Flash log</strong>
          <button id="installer-copy-log" type="button" aria-label="Copy flash log to clipboard">Copy log</button>
          <span id="installer-log-action-status" role="status" aria-live="polite"></span>
        </div>
        <pre id="installer-log-output" role="log" aria-label="Installer flash log" aria-live="off" tabindex="0"></pre>
      </div>
    </div>
  </div>
</div>

<script type="module" src="../assets/javascripts/installer.mjs?v=installer-ui-9"></script>

## Update HomeTiles

For a display that already runs HomeTiles:

1. Under **Firmware**, use the published release or select a local HomeTiles
   `.bin` test build.
2. Select the exact **Device**.
3. Choose **Update** and confirm the model label.
4. Select **Connect and flash**, then choose the display's serial port.
5. Keep the page and cable connected until **Complete** appears.

Update preserves Wi-Fi, MQTT, tiles, NVS, and LittleFS. When the display is
online, the on-device updater under **Settings → System** is the simplest
alternative; the browser Update is useful for a local file or USB recovery.

<details class="ht-installer-log ht-installer-doc-details" markdown="1">
<summary>Update safety and partition details</summary>

Before writing, the installer checks:

- ESP32-P4 or ESP32-S3 and the flash size,
- the current HomeTiles partition layout and OTA selection,
- the firmware's device ID and SHA-256 digest.

The installer writes and verifies only the inactive application slot:

| Partition or data | Offset | Update behavior |
| --- | ---: | --- |
| Currently selected app slot | `0x10000` or `0x690000` | Preserved |
| Inactive app slot | `0x10000` or `0x690000` | Written and verified |
| Redundant `otadata` | `0xE000` / `0xF000` | Boot entry committed after app verification |
| `nvs` | `0x9000` | Not written |
| `spiffs` / LittleFS | `0xD10000` | Not written |

There is no full-chip erase. Wi-Fi, MQTT, tiles, NVS, and LittleFS remain in
place. If a check fails, the installer stops before writing. The boot selection
changes only after the inactive slot has been fully written and verified.

**Keep power, USB, and the browser connected until completion.** If an Update
is interrupted, the previously selected app remains available; restart to keep
using it or reconnect and run Update again.

**Hardware validation:** The installer validates files, chip family, flash
size, and the HomeTiles partition contract. Physical display, touch, storage,
and networking validation remains separate.

</details>

## First install or factory reset

Use this only for a new device or an intentional clean start:

1. Select the exact device and choose **First install / factory reset**.
2. Confirm both the model and the erase warning.
3. Connect the serial port and wait for **Complete**.

This erases the entire flash and writes the matching `_factory.bin` at `0x0`.
Export the configuration first if it may be needed again.

## Manual flashing

Manual flashing is the fallback for a first installation or complete reset.
For a normal update, use **Update HomeTiles** above or the on-device updater;
do not guess an OTA-slot address in a desktop tool.

1. Download the exact `_factory.bin` for the device from the
   [latest HomeTiles release](https://github.com/GalusPeres/HomeTiles/releases/latest).
2. Open Espressif's [Flash Download Tool](https://www.espressif.com/en/support/download/other-tools),
   select the correct chip family, and choose **UART**.
3. Select the `_factory.bin`, set the address to **`0x0`**, choose the serial
   port, and start flashing.
4. Wait for **FINISH**, then restart the display.

Use **ESP32-P4** for P4 displays and **ESP32-S3** only for the Guition
ESP32-4848S040C_I. The plain `.bin` is an Update image and must not be written
to `0x0`. The [manual flashing guide](flashing.md) contains the complete tool
and command-line instructions.

## Troubleshooting

1. Make sure the cable carries data, not only power, and try another USB port.
2. Close every serial monitor or flashing tool using the port.
3. Select the port that appears when the display is connected.
4. If automatic reset fails, hold the device's **BOOT** button while clicking
   **Connect and flash**, then release it once the connection starts.
5. Do not choose a "similar" P4 panel. The browser can reject P4/S3 and flash-size
   mismatches, but it cannot electrically distinguish every P4 display model.

<details class="ht-installer-log ht-installer-doc-details" markdown="1">
<summary>Local test before publication</summary>

The local test continues to use the unchanged, SHA-256-verified assets from the
currently published GitHub release. With `--device`, only the two files for the
selected device are downloaded. Without that option, the published site always
contains all nine device profiles.

1. Build the documentation:
   `python -m mkdocs build --strict`
2. Prepare the exact profile, for example the Guition ESP32-S3:
   `node release-helper/prepare-web-installer.mjs --output site/firmware/latest --device guition_esp32_4848s040`
3. Start a local server from the repository:
   `python -m http.server 8000 --directory site`
4. Open `http://127.0.0.1:8000/installer/` in desktop Chrome or Edge. Browsers
   treat the loopback address as a secure context for Web Serial; opening the
   generated HTML file directly does not work.
5. Select the device, test **Update** first, confirm the exact hardware, and
   choose **Connect and flash**. Test **Factory reset** only when erasing every
   setting is intentional.

The other valid `--device` values match the release file names:
`m5stacks_tab5`, `waveshare_4b`, `waveshare_touch_lcd_7`,
`waveshare_touch_lcd_8`, `waveshare_touch_lcd_10_1`,
`guition_jc8012p4a1`, `guition_jc8012p4a1_v2`,
`guition_jc1060p470c`, and `guition_esp32_4848s040`.

Implementation references: [ESP Web Tools](https://esphome.github.io/esp-web-tools/),
[esptool-js](https://github.com/espressif/esptool-js), and Espressif's
[esptool documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32/basic-commands.html).

</details>
