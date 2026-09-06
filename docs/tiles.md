# Tile Types

Create and configure tiles in the [Web Admin](web-admin.md#creating-a-tile). Select Home Assistant entities in the [Bridge options](bridge.md#entity-configuration) first.

<a id="create-a-tile"></a>

**Home Assistant tiles** show entity data or control your devices. **Local tiles** provide clocks, text, folders, animations, and spacing.

All tiles share title, icon, color, size, and position settings. For types with a popup, choose whether a tap or long press opens it. See [on-device controls](device-ui.md#popups) for screenshots.

## Home Assistant Tiles

### Sensor

Shows a numeric or text-valued `sensor.*` entity. Configure the unit, decimals, value size, and an optional gauge with a minimum and maximum.

**Popup:** 24-hour or 7-day history as a graph for numbers, or a timeline and Activity list for text states.

A local [DS18B20 input](hardware-io.md) can also supply the value without Home Assistant; local inputs have no Home Assistant history popup.

### Binary Sensor

Shows a `binary_sensor.*` entity with the matching state and icon, such as Open/Closed or Motion/Clear. Unknown and unavailable states are shown separately.

**Popup:** a 24-hour or 7-day state timeline and Activity list.

### Energy

Shows Home Assistant **Energy Dashboard** statistics for electricity, gas, water, or cost. Enable the matching [Bridge energy category](bridge.md#energy-dashboard).

Configure the energy entity, unit, decimals, and value size. In this example, Sensor tiles show current power at the top; Energy tiles show today's totals at the bottom.

<figure class="ht-screenshot">
<img src="../images/8in-folder-pv.png" alt="Sensor tiles on top, energy tiles at the bottom" width="1308" height="828" loading="lazy">
<figcaption>Solar dashboard with sensor and energy tiles</figcaption>
</figure>

**Popup:** hourly bars for the day and daily bars for the week.

### Switch

Toggles a `switch` or `light` entity and reflects its state. Choose the entity, tile style, and popup trigger.

Local [outputs and relays](hardware-io.md) appear in the same selector and work without Home Assistant.

**Popup for lights:** supported brightness, color, and color-temperature controls.

### Cover

Controls a `cover` entity and shows its state and position when available.

**Popup:** supported position and tilt sliders, plus open, close, and stop buttons.

### Scene

Runs a scene or script with a tap; there is no popup. Choose the alias generated for it in the [Bridge options](bridge.md#entity-configuration).

### Weather

Shows current conditions from a `weather` entity.

**Popup:** temperatures, precipitation, and rain probability for the available forecast.

### Media

Shows cover art, title, and playback controls for a `media_player` entity.

**Popup:** playback controls and volume.

### Climate

Controls a `climate` entity. The icon and accent indicate active heating, cooling, drying, or fan operation.

Configure mini-tiles for temperatures, humidity, targets, and mode, or choose **Automatic**. Arrange them in the [Climate mini-tile editor](web-admin.md#editing-climate-mini-tiles).

<figure class="ht-screenshot">
<img src="../images/8in-climate.png" alt="Climate tiles with several mini-tile layouts" width="1308" height="828" loading="lazy">
<figcaption>Climate tiles with different mini-tile layouts</figcaption>
</figure>

**Popup:** temperature controls and supported modes, presets, fan, swing, and humidity settings. Heating/cooling ranges have separate targets.

### Camera (experimental) { data-toc-label="Camera" }

Opens a 16:9 video popup on ESP32-P4. Select the camera in the Bridge's **Entity Configuration**, then assign it to this tile.

Allow local TCP access to the Home Assistant host on ports `8124`–`8131`. See [Camera connection](bridge.md#experimental-camera-transport) for requirements and performance notes. Camera tiles are unavailable on ESP32-S3.

## Local Tiles

These types work without Home Assistant.

### Clock

Shows time and date using the device's localization settings. You can override the formats and sizes for each tile. Tap it to open the [screensaver](screensaver.md).

### Text

A static label with a selectable font size.

### Folder

Opens a sub-page with its own grid and an automatic back tile. See [Folders](web-admin.md#folders) for setup and optional PIN protection.

### Animation

Plays a `.panim` pixel animation from `/animations` on microSD. Configure frame rate, fit, and zoom.

### Empty

Leaves an empty cell for spacing.
