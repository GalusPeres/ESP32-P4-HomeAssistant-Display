# Screensaver

HomeTiles v0.5.0 adds a separate screensaver layout with an image slideshow,
a freely positioned clock, and optional Home Assistant tiles. It is configured in
the **Screensaver** tab of the web admin and runs on every supported display.

![Screensaver on the Waveshare 8 inch display](images/8in-screensaver.png){ width="82%" }

## Prepare the microSD Card

1. Format a microSD card as FAT32 and insert it into the display.
2. Create a folder named `images` in the root of the card, so its path is `/images`.
3. Copy JPEG files (`.jpg` or `.jpeg`) into that folder. The web admin file manager
   can upload several selected files at once.
4. Open or reload the **Screensaver** tab. The files appear in the **Images** list.

### Image Requirements

- **Format:** baseline (non-progressive) JPEG with a `.jpg` or `.jpeg` extension
- **Color:** normal RGB/sRGB export; do not use CMYK JPEG
- **Size:** resize to the native resolution of the selected display whenever
  possible. Smaller source images decode faster and need less temporary memory.
- **One shared set for several displays:** keep the longest edge at **1920 pixels
  or less** while preserving the aspect ratio.
- **Maximum file size:** 8 MB per image; a native-resolution JPEG at quality
  80-90 should normally be well below 2 MB.

| HomeTiles target | Native landscape image size |
| --- | ---: |
| M5Stack Tab5 | 1280×720 |
| Waveshare 4B / 86 Panel | 720×720 |
| Waveshare Touch LCD 7 inch | 1280×720 |
| Waveshare Touch LCD 8 inch | 1280×800 |
| Waveshare Touch LCD 10.1 inch | 1280×800 |
| Guition JC8012P4A1 V1 / V2 | 1280×800 |
| Guition JC1060P470C_I_W_Y | 1024×600 |
| Guition ESP32-4848S040C_I | 480×480 |

If the display is deliberately rotated by 90 degrees, swap width and height.
The screensaver crops the prepared image to the panel; **Zoom**, **Focus X**, and
**Focus Y** control that crop.

!!! warning "Baseline JPEG is required"
    HomeTiles' software JPEG decoder supports baseline JPEG, not progressive
    JPEG. In the image export dialog, choose **JPEG**, **RGB/sRGB**, and
    **Baseline / Standard**, and disable **Progressive** or **Interlaced**
    encoding. A file can have the correct `.jpg` extension and still be
    incompatible because of its encoding or color mode.

### Reliable Export Recipe

1. Crop the photo to roughly the panel's aspect ratio.
2. Resize it to the native resolution in the table. For a shared multi-device
   image, preserve the aspect ratio and cap the longest edge at 1920 pixels.
3. Convert the document to RGB/sRGB if the editor offers a color-mode choice.
4. Export as baseline/non-progressive JPEG at quality 80-90.
5. Confirm the file is below 8 MB, then copy or upload it to `/images`.

Do not rely on a very large camera image being resized by the display. In
[issue #20](https://github.com/GalusPeres/HomeTiles/issues/20), a 3072×2304 JPEG
stayed blank on a JC1060P470C while a 2000×1500 export worked. The conservative
recommendation remains the device's native resolution, or at most a 1920-pixel
long edge for a shared image set.

If an image remains black, re-export the original as baseline RGB JPEG at the
native size first. If needed, reduce it further; for the reported JC1060 case,
2000×1500 or smaller was the first confirmed working range. Check the format
and pixel dimensions before treating this as a general microSD upload problem.

Without a card or usable JPEG, the screensaver still works with a black background
and the configured clock and tiles.

## Open It Manually or Automatically

- Tap any **Clock** tile on the normal dashboard to open the screensaver immediately.
- To activate it after inactivity, open **Settings → Display** on the device and set
  the **Screensaver** slider. This timeout is separate from the display sleep timeout.
- Use **Saver bright.** in the same popup for an independent 1-100% screensaver
  brightness (default 25%). Dragging previews the level; normal brightness
  returns after release.
- Tap the free image/background area to return to the dashboard.

![Display settings with screensaver timeout](images/8in-display-popup-screensaver.png){ width="70%" }

## Configure the Slideshow

Open `http://<display-ip>/` and select the **Screensaver** tab. Click the image
background to select the slideshow settings.

![Screensaver editor in the web admin](images/web-admin-screensaver.png)

| Setting | What it does |
| --- | --- |
| **Use images** | Enables the slideshow background; disabled means black background |
| **Shuffle** | Chooses the next enabled image randomly instead of using list order |
| **Tile shadows** | Adds shadows below the overlay tiles |
| **Tile borders** | Adds a subtle modern outline around overlay tiles |
| **Image checkboxes** | Include or exclude individual JPEG files |
| **Arrow buttons** | Set the slideshow order |
| **Duration** | Global display time in seconds for every image |
| **Zoom** | Enlarges the selected image while preserving its aspect ratio |
| **Focus X / Focus Y** | Moves the crop horizontally or vertically |

Changes save automatically and are applied live on the display.

## Position the Clock

Click the clock in the preview to open its settings. Drag the clock anywhere on
the image and use its resize handle to set the available text area; it is not tied
to the tile grid. Time and date can be enabled independently, with an optional
weekday, separate font sizes and formats, left/center/right alignment, and text
shadow.

## Add Tiles

The bottom two rows are regular screensaver tile slots. Select an empty slot, choose
Sensor, Energy, Switch, Scene, or Media, then drag and resize it exactly as on a
normal folder page. Color and **Opacity** are configured together; the reset button
restores both. Sensor and Energy values remain live while the screensaver is open.
Popups are not opened in screensaver mode, so touches act directly on supported
controls or leave the overlay stable.

Hardware Switch and temperature entities configured on the same panel are also
available in these bottom rows. They continue to work locally when Home
Assistant is offline.

!!! warning "Media tiles"
    Media tiles are supported and use a minimum size of 2×2. Loading and decoding
    cover art adds WiFi, RAM, and rendering work, so entering the screensaver or
    changing slides can be slower than with Sensor, Energy, or Switch tiles. On
    memory-constrained layouts, use only one media tile or omit it for the smoothest
    slideshow.

## Backup and Restore

Web-admin **Export** includes the screensaver layout and settings together with all
folders and tiles. Imports from older HomeTiles versions remain compatible: if an
old JSON file has no screensaver section, the current screensaver is left unchanged.
