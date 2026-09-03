# Tile Types

Everything on the dashboard is a tile on a grid. Tiles are created, moved, resized, and
configured in the [web admin panel](web-admin.md) — click a cell, pick a type from the
dropdown, fill in its fields:

![Tile type dropdown in the web admin](images/web-admin-tile-types.png){ width="340" }

Tiles that show Home Assistant data need their entity to be exposed through the
[bridge integration](bridge.md) first. Most data tiles open a detail popup — whether on
a **tap** or a **long press** is configurable per tile in the web admin.

## Home Assistant Tiles

### Sensor

Shows the current value of a Home Assistant `sensor.*` entity, with icon,
title, and unit.

It can also show a DS18B20 input configured on this panel's
[I/O page](hardware-io.md). That value is sampled locally and remains
available without Home Assistant; the panel does not request a Home Assistant
history chart for a local input.

**Config:** entity, unit, decimals, value size, and an optional display mode that turns
the tile into a gauge (with min/max range).

**Popup:** numeric entities use the established chart with a 24-hour and a
7-day view. Text-valued entities use a categorical timeline and date-grouped
Activity list instead. The data is fetched live from Home Assistant through
HomeTiles Bridge v0.6.40 or newer.

![Sensor history popup](images/8in-sensor-popup-7d.png){ width="65%" }

### Binary Sensor

Shows a Home Assistant `binary_sensor.*` entity. The displayed state and icon
follow its Home Assistant device class, so a single on/off value can appear as
Detected/Clear, Open/Closed, Motion/Clear, Wet/Dry, or another localized pair.
Unknown and unavailable states remain distinct.

**Config:** Binary Sensor entity and whether the popup opens on a short press
or long press. Title, icon, color, size, and position use the normal shared tile
settings. Changes update the preview, save automatically, and appear on the
display like other tile types.

**Popup:** a 24-hour or 7-day colored state timeline followed by a scrollable,
date-grouped Activity list. HomeTiles Bridge v0.6.40 or newer supplies the
bounded history data.

### Energy

Shows a value from the Home Assistant **Energy Dashboard** — solar yield, grid
import/export, battery, gas, water, or cost/savings. Requires the matching energy
category to be enabled in the [bridge options](bridge.md#energy-dashboard) and a
configured Energy Dashboard in Home Assistant.

On the grid, energy tiles look exactly like sensor tiles — the difference is where the
data comes from and what the popup shows. Here, the tiles at the top are **sensor**
tiles (live power right now), the bottom row are **energy** tiles (statistics for the
day so far):

![Sensor tiles on top, energy tiles at the bottom](images/8in-folder-pv.png){ width="75%" }

**Config:** energy entity, unit, decimals, value size.

**Popup:** bar charts from the energy statistics — hourly bars in the day view, daily
bars in the week view:

![Energy popup day view](images/8in-energy-24h.png){ width="49.5%" }
![Energy popup week view](images/8in-energy-7d.png){ width="49.5%" }

### Switch

Toggles a `switch` or `light` entity with a tap; the tile reflects the current state.

Switch outputs and supported onboard relays configured on this panel's
[I/O page](hardware-io.md) appear in the same selector and toggle directly,
without waiting for an MQTT round trip.

**Config:** entity, tile style, popup trigger.

**Popup (lights):** the full light control set — brightness slider, color wheel, and
color temperature. The icon row at the bottom switches between the views; views only
appear if the light supports them.

![Light popup brightness](images/8in-light-brightness.png){ width="32.8%" }
![Light popup color wheel](images/8in-light-color.png){ width="32.8%" }
![Light popup color temperature](images/8in-light-temperature.png){ width="32.8%" }

### Cover

Controls a Home Assistant `cover` entity. The tile shows the localized state
and, when available, the current position.

**Config:** Cover entity and popup trigger.

**Popup:** separate position and tilt sliders plus open, close and stop buttons.
HomeTiles uses the entity's `supported_features` mask, so only controls actually
provided by Home Assistant appear. Missing position or tilt values remain
unknown instead of being displayed as zero.

### Scene

Triggers a scene or script with a tap — no popup. The tile references the **scene
alias** defined in the bridge integration (aliases are generated automatically when you
select scenes/scripts in the bridge options, or mapped manually there as
`alias=entity_id`). The scene tiles in the folder screenshot further down
(*Bright*, *Reading*, *Warm*, ...) are typical examples.

### Weather

Shows current conditions from a `weather` entity.

**Popup:** the forecast ahead — daily min/max with an hourly temperature curve,
precipitation, and rain probability; the arrows page through the coming weeks:

![Weather popup](images/8in-weather-popup.png){ width="65%" }

### Media

Controls a `media_player` entity: cover art, current title, and playback controls
directly on the tile.

**Popup:** the full control set including previous/next and a volume slider:

![Media popup](images/8in-media-popup.png){ width="65%" }

### Climate

Controls a Home Assistant `climate` entity for heating, cooling, or air
conditioning. The tile changes its icon and accent color when the device is
actively heating, cooling, drying, or running its fan.

**Config:** climate entity, popup trigger, and a configurable grid of mini-tiles.
Each mini-tile can show the current temperature, current humidity, target
temperature, heating target, cooling target, target humidity, or HVAC mode.
Choose **Automatic** to select suitable content from the entity's reported
capabilities, or arrange the individual values yourself.

Mini-tiles can be selected and moved directly in the web preview. Resizing the
parent Climate tile changes the available slot grid while preserving explicitly
configured content wherever possible. Unsupported controls are not added by the
automatic layout.

![Climate tiles with several mini-tile layouts](images/8in-climate.png){ width="80%" }

**Popup:** a circular target-temperature control with plus/minus buttons and
buttons for every control reported by the entity. Depending on its capabilities,
this can include HVAC mode, presets, fan mode, vertical swing, horizontal swing,
and target humidity. `heat_cool` entities expose a low/high target range with
separate controls.

![Full Climate popup](images/8in-climate-popup-1.png){ width="49.5%" }
![Heat-only Climate popup](images/8in-climate-popup-2.png){ width="49.5%" }

### Camera (experimental)

Opens a 16:9 camera popup on supported ESP32-P4 displays. Select the `camera`
entity in **HomeTiles Bridge → Configure → Entity Configuration** first, then
assign it to a Camera tile in the web admin.

Camera tiles require:

- HomeTiles firmware v0.6.3 or newer
- HomeTiles Bridge v0.6.28 or newer
- Home Assistant's FFmpeg and camera integrations
- local TCP access from the display to the Home Assistant host on one port in
  the range `8124`–`8131`

The bridge accepts either a direct stream source or a still-image camera. It
transcodes the source into display-sized JPEG frames and sends only as quickly
as the panel acknowledges them. Direct sources can reach up to 24 FPS; actual
frame rate is limited by the camera source, Home Assistant host, network and
display profile. Snapshot-only cameras update at their own image refresh rate.

The feature is experimental. Video transcoding consumes CPU on the Home
Assistant host, and opening camera popups on several panels starts several
transcoding sessions. Camera tiles are intentionally unavailable on the
ESP32-S3 Guition ESP32-4848S040C_I and Waveshare ESP32-S3-Touch-LCD-4B
targets.

## Local Tiles

These tiles work without Home Assistant.

### Clock

Time and date. Follows the device's localization settings (language, time zone,
12h/24h); per tile you can toggle time/date separately, set their sizes, and override
the formats.

### Text

A static text tile with selectable font size — useful for headings and labels on
the grid.

### Folder

Opens a sub-page with its own tile grid; a back tile is placed there automatically.
Use folders to group lights, rooms, or feature areas — like this lighting page with
its light switches and scene tiles:

![Folder page with light tiles and scenes](images/8in-folder-lighting.png){ width="75%" }

Creating one is a single step — see [Folders](web-admin.md#folders) in the web
admin guide.

### Animation

Plays a low-res pixel-art animation from a `.panim` file in the `/animations` folder
of the microSD card — a purely decorative element. Frame rate, fit, and zoom are
configurable.

### Empty

A spacer tile for layout purposes.
