#ifndef BOOT_SPLASH_H
#define BOOT_SPLASH_H

// Brief full-screen greeting during setup(), with the same icon,
// HomeTiles name, version and device branding as the system popup.
// Requires successful displayManager.init(), which creates the active
// LVGL screen. It does not display actual boot progress; show it long
// enough to read the version/device before entering the normal UI
// (see the minimum duration around hide() in HomeTiles.ino).
namespace BootSplash {

// Create the LVGL objects only. After displayManager.init(), the panel
// is not visible yet because displayWake() has not run. The caller must
// immediately wake the display through BoardHAL and refresh it; see
// HomeTiles.ino for the device-specific wake helpers, which are not
// exported to this module.
void show();

// Remove the overlay completely by deleting its LVGL objects. Call only
// after the splash has been visible long enough. The caller then builds
// the real UI on the same screen; the splash and tiles must not overlap.
void hide();

}  // namespace BootSplash

#endif  // BOOT_SPLASH_H
