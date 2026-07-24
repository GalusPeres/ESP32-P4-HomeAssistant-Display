# Home Assistant Setup Guide

This guide connects a HomeTiles display to Home Assistant, starting from scratch.
No YAML editing is required.

!!! note "What you need"
    - Home Assistant (any recent install type)
    - A display with the [firmware flashed](flashing.md)
    - 15 minutes

## How It Works

<div class="ht-flow">
  <span class="ht-node">Display</span>
  <span class="ht-link">←&thinsp;MQTT&thinsp;→</span>
  <span class="ht-node">MQTT Broker</span>
  <span class="ht-link">←&thinsp;MQTT&thinsp;→</span>
  <span class="ht-node">Bridge Integration<small>Home Assistant</small></span>
</div>

The display never talks to Home Assistant directly. Everything goes through MQTT:

- The **bridge integration** pushes entity states, icons, weather forecasts, sensor history,
  and energy data to the display.
- The display sends commands back (light/switch/media/scene control), which the bridge
  executes in Home Assistant.

So you need three things: an **MQTT broker**, the **bridge integration**, and the
**display firmware**. This guide covers all three.

## Step 1: Set Up An MQTT Broker

If you already have a working MQTT broker, skip to Step 2.

The easiest option on Home Assistant OS is the official Mosquitto add-on:

1. In Home Assistant, go to **Settings → Add-ons → Add-on Store**.
2. Search for **Mosquitto broker**, install it, and start it.
   Enable **Start on boot** and **Watchdog**.
3. Create a Home Assistant user for the display:
   **Settings → People → Users → Add user** (for example `display`, with a password of
   your choice). The Mosquitto add-on accepts Home Assistant users as MQTT credentials.
4. Set up the MQTT integration: after the add-on starts, Home Assistant usually discovers
   it automatically — go to **Settings → Devices & Services**, look for the discovered
   **MQTT** integration, and confirm it. If it is not discovered, add the **MQTT**
   integration manually and use `core-mosquitto` as the broker address.

## Step 2: Install The Bridge Integration

The bridge is a custom integration: [HomeTiles Bridge](https://github.com/GalusPeres/HomeTiles-Bridge)

### Via HACS (recommended)

1. **HACS → Integrations → three-dot menu → Custom repositories**
2. Repository: `https://github.com/GalusPeres/HomeTiles-Bridge`,
   category **Integration**, click **Add**.
3. Search for **HomeTiles Bridge** in HACS and download it.
4. Restart Home Assistant.

### Manual

1. Copy the `custom_components/tab5_lvgl` folder from the bridge repository into your
   Home Assistant `custom_components` directory.
2. Restart Home Assistant.

## Step 3: Add The Integration

1. Go to **Settings → Devices & Services → Add Integration**.
2. Search for **HomeTiles Bridge**.
3. Keep the defaults unless you have a reason not to — the details (base topic,
   HA prefix) are explained on the [bridge page](bridge.md).

## Step 4: Get The Display Online

On the display, open **Settings → WiFi**. Either:

- pick your network from the scan list and enter the password with the on-screen
  keyboard, or
- tap **Enable AP**: the display starts a hotspot (password `12345678`, QR code shown
  on screen) — connect to it and enter your WiFi credentials in the captive portal.

## Step 5: Pair The Display

As long as the display has **no MQTT credentials configured yet**, it announces itself
on the network automatically. In Home Assistant, a **discovered device** card appears
under **Settings → Devices & Services** — confirm it, and the bridge pushes your MQTT
broker's credentials to the display. No typing required.

Afterwards the display shows up as a device under the bridge integration:

![Panels as devices in the bridge integration](images/bridge-devices.png){ width="88%" }

??? info "Manual alternative: enter MQTT settings yourself"
    If you prefer manual setup (or discovery is blocked in your network), open the
    display's web admin panel at `http://<display-ip>/` (the IP is shown in the
    on-device WiFi settings) and enter under **Settings → MQTT**:

    - **Host**: the IP address of your Home Assistant machine (when using the
      Mosquitto add-on, the broker runs there)
    - **Port**: `1883`
    - **Username / Password**: the Home Assistant user from Step 1
    - Leave **Device topic base** and **Home Assistant prefix** at their defaults
      unless you changed them in the bridge

    Save — the display restarts and connects to the broker.

!!! tip "Display missing in Home Assistant later?"
    The on-device **Settings → System → Pairing** button re-announces the display at
    any time — useful if you deleted the device in Home Assistant and want it back.

## Step 6: Choose What The Display Can See

Open the integration entry and click **Configure**. There are three sections:

- **Panel Settings** — base topic, HA prefix, and device metadata.
- **Entity Configuration** — pick the sensors, weather entities, lights, switches,
  media players, and scenes/scripts the display should have access to. Scene aliases are
  generated automatically; you can also map them manually (one `alias=entity_id` per line).
- **Energy Dashboard** — enable electricity, gas, and/or water. This requires the
  Home Assistant [Energy Dashboard](https://my.home-assistant.io/redirect/energy/) to be
  set up; the display's energy tile pulls its statistics from there.

Entity selections are shared across all panels — every display can use every entity you
pick here.

## Step 7: Build Your Dashboard

Open the display's [web admin panel](web-admin.md) and add tiles for the entities you
exposed in Step 6 — sensors, lights, weather, energy, media, scenes, and so on.
Changes appear on the display immediately.

## Multiple Displays

- Each display needs its **own base topic** (for example `hometiles`, `panel_kitchen`, ...) —
  set in the display's MQTT settings, or handled automatically during pairing.
- The **HA prefix stays the same** for all displays.
- Additional displays are discovered automatically: once the first panel is set up, any new
  display that connects gets its own integration entry without manual steps.

## Troubleshooting

**The display connects to WiFi but shows no data**

- Check the MQTT settings on the display (host, port, credentials).
- Make sure base topic and HA prefix match between the display and the integration entry.
- Check **Settings → Devices & Services** — the panel should be listed under the bridge
  integration. If not, tap **Settings → System → Pairing** on the display; it
  re-announces itself.

**No "discovered device" card appears**

- Discovery only runs while the display has no MQTT credentials stored. If it had some
  before, either use the manual setup above or factory-flash the device.
- If the device existed in Home Assistant before, delete the old device entry first —
  its ID survives a re-flash, and Home Assistant won't re-discover a known ID.

**Lights/switches/sensors are missing on the display**

- They must be selected in the bridge options first (Step 6), then assigned to a tile in
  the web admin (Step 7).

**The energy tile stays empty**

- The Home Assistant Energy Dashboard must be configured, and the matching category
  (electricity/gas/water) must be enabled in the bridge's Energy options.

**MQTT login fails**

- When using the Mosquitto add-on, the credentials are a regular Home Assistant user
  (Step 1), not an add-on-specific account.
