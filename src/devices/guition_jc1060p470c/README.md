# Guition JC1060P470C

Experimental HomeTiles target for the 7-inch `JC1060P470C_I_W_Y`.

This target currently claims the `_I_W_Y` variant only. Guition documents the
`_I_W` and `_I_W_Y` suffixes as different hardware variants; its examples use
direct 1024x600 touch coordinates for `_I_W_Y`, while `_I_W` scales an
800x480 touch range to the panel resolution.

Hardware configuration:

- ESP32-P4 with 16 MB flash and 32 MB PSRAM
- 1024x600 JD9165 MIPI-DSI display
- GT911 touch on I2C0, SDA GPIO7, SCL GPIO8
- LCD reset GPIO27, touch reset GPIO22, touch interrupt GPIO21
- Backlight PWM on GPIO23, active high
- MIPI PHY LDO channel 3 at 2.5 V
- TF/SD card on SDMMC slot 0 with LDO channel 4

The HomeTiles UI uses the independently retained `DEVICE_LAYOUT_1024X600`
layout profile. The panel itself is native landscape, so the default display
path writes directly to the framebuffer; the flipped orientation uses a
separate 180-degree transform.

The JD9165 driver and SDMMC power setup are derived from Espressif files under
Apache-2.0. See `vendor/README.md` and `vendor/LICENSE-APACHE-2.0.txt`.

Hardware references:

- https://github.com/Deep-start9527/guition_product_demo/tree/62e51b0449ac520031520361f71bab7699484fe4/demo_arduino/JC1060P470C_I_W_Y
- https://github.com/cheops/JC1060P470C_I_W/tree/6304eddef32a33aa9d9a85e971e39dfb24c3cca3
