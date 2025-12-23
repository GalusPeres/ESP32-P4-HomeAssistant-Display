#ifndef KEY_PARSING_H
#define KEY_PARSING_H

#include <Arduino.h>

/**
 * Parses a key macro string (e.g., "ctrl+alt+f1") into a USB HID key code and modifier mask.
 *
 * @param macro The string to parse (e.g., "ctrl+c"). Case-insensitive.
 * @param keyCode [out] The resulting USB HID usage ID for the key (0 if invalid).
 * @param modifier [out] The resulting modifier bitmask (0x01=Ctrl, 0x02=Shift, 0x04=Alt).
 */
void parseKeyMacro(String macro, uint8_t& keyCode, uint8_t& modifier);

#endif
