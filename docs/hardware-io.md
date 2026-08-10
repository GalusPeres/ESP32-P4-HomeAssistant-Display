# Local Hardware I/O

HomeTiles v0.6.4 can assign up to eight profile-whitelisted device GPIOs to
local switches or DS18B20 temperature sensors. Configuration lives on the
panel and the runtime path is local: a Switch tile can toggle an assigned
output even when Home Assistant or MQTT is offline.

Open the panel's web admin and select **I/O**. Each assignment is shown as
one compact row with its HomeTiles entity ID.

Hardware assignments are stored separately from dashboard folders and tiles.
They are therefore not included in a dashboard export/import file; configure
them explicitly on each physical panel.

## Switch Outputs

1. Select **+ Switch**.
2. Enter the name that should appear on the panel and in Home Assistant.
3. Select one of the GPIOs offered for this exact firmware target.
4. Choose **High** or **Low** as the active output level when the pin is
   configurable.
5. Choose whether the output starts **Off** or **On** after a restart.
6. Select **Save** to apply it immediately. **Restart** reboots separately and
   does not save pending edits.

The ESP32-P4-86-Panel-ETH-2RO onboard relays are available as Switch outputs in
the shared `waveshare_4b` firmware. **+ Switch** starts with a normal P3 GPIO;
select `Relay 1` or `Relay 2` explicitly in the GPIO list only on the exact
86-Panel-ETH-2RO hardware. Their active-high polarity is fixed by the device
profile. **On** energizes the relay and closes NO-COM; **Off** releases it and
closes COM-NC. The relay-equipped Guition ESP32-4848S040 variant exposes its
first onboard relay in the experimental ESP32-S3 firmware.

## DS18B20 Temperature Inputs

1. Select **+ Temperature**.
2. Enter a name and choose a compatible GPIO.
3. Select the displayed precision and save.

HomeTiles samples the sensor locally and makes the value available to Sensor
tiles on the normal dashboard and screensaver. The implementation uses
Skip-ROM, so configure exactly one DS18B20 per assigned GPIO/1-Wire bus. The
internal ESP32 pull-up is deliberately disabled: the sensor needs external
power and a 4.7 kOhm pull-up from its data line to 3.3 V.

## Tiles And Home Assistant

After **Save**, the new local entity is available immediately in the matching
Switch or Sensor entity selector on the same panel. This does not require the
Bridge.

With HomeTiles Bridge v0.6.32 or newer, the panel also announces each assignment
as an entity on that panel's Home Assistant device:

- Switch outputs use the `switch` domain.
- Temperature inputs use the `sensor` domain.
- The visible entity ID is derived from the device and channel name.
- The internal channel identity stays stable for MQTT topics and Home Assistant
  unique IDs.

When an automatically generated local entity ID changes during an upgrade, the
Bridge migrates known old IDs. An entity ID that was manually renamed in Home
Assistant is left untouched. Multiple identical panels remain separate Home
Assistant devices and Home Assistant resolves any remaining visible ID collision
with its normal numeric suffix.

## Available Pins

The I/O page does not show every ESP32 GPIO. It only offers pins which are
whitelisted by that firmware target's device profile and are not known to be
reserved for display, touch, storage, audio, camera, boot, USB, or ESP-Hosted
networking. Some experimental profiles are schematic-derived and still need
community hardware validation; a successful firmware build does not validate
electrical suitability.

| Firmware target | Offered local I/O | Validation |
| --- | --- | --- |
| M5Stack Tab5 | GPIO EXT 0, 1, 49, 50; Port A 53, 54; M5-Bus 2, 3, 4, 16, 17, 45, 47, 48, 51, 52 | Profile-whitelisted; attached Units and M5-Bus modules can still create conflicts |
| Waveshare 4B | P3 GPIO 2, 3, 4, 5, 21; 86 Panel relays GPIO 32 and 46 | P3 schematic-derived; onboard relays only for the exact 86-Panel-ETH-2RO |
| Waveshare 7 inch | GPIO 2, 3, 4, 5, 21, 22, 28, 29, 30, 31, 32, 34, 46, 47, 48, 49, 50, 51, 52 | Experimental / community validation needed |
| Waveshare 8 inch | GPIO 2, 3, 4, 5, 21, 22, 28, 29, 30, 31, 32, 34, 46, 47, 48, 49, 50, 51, 52 | Profile-whitelisted; verify attached hardware |
| Waveshare 10.1 inch | GPIO 2, 3, 4, 5, 21, 22, 28, 29, 30, 31, 32, 34, 46, 47, 48, 49, 50, 51, 52 | Experimental / community validation needed |
| Guition JC8012P4A1 | Expansion FPC GPIO 2, 3, 4, 5, 28, 29, 30, 31, 32, 33, 34, 45, 46, 47, 48 | Schematic-derived; verify the connected expansion board |
| Guition JC8012P4A1 V2 | Expansion FPC GPIO 2, 3, 4, 5, 28, 29, 30, 31, 32, 33, 34, 45, 46, 47, 48 | Separate V2 profile; same reported board wiring, community validation ongoing |
| Guition JC1060P470C | Expand GPIO 1, 2, 3, 4, 5, 20, 32, 33, 46, 47 | GPIO45 is reserved for the active-low SD-card power switch; experimental / community validation needed |
| Guition ESP32-4848S040 | Onboard Relay 1 on GPIO 40 for relay-equipped variants | Experimental; relay-equipped variants only |

!!! danger "3.3 V logic only"
    Never connect a relay coil, mains voltage, or another high-current load
    directly to an ESP32 GPIO. Use a 3.3 V-compatible driver or relay module and
    follow the hardware manufacturer's schematic. Do not assign a pin already
    used by an attached module.
