#ifndef MDI_ICONS_H
#define MDI_ICONS_H

#include <Arduino.h>
#include <lvgl.h>

// Font-Deklaration (muss extern definiert werden, z.B. in main .ino)
extern const lv_font_t mdi_icons_48;
#define FONT_MDI_ICONS (&mdi_icons_48)

// Icon-Name zu Unicode-Codepoint Mapping
// Gibt den Codepoint zurück für einen Icon-Namen (z.B. "home" -> 0xF02DC)
uint32_t getMdiCodepoint(const String& iconName);

// Gibt ein String mit dem Unicode-Zeichen zurück (für lv_label_set_text)
String getMdiChar(const String& iconName);

#endif // MDI_ICONS_H
