#include "key_parsing.h"

void parseKeyMacro(String macro, uint8_t& keyCode, uint8_t& modifier) {
  macro.trim();
  macro.toLowerCase();

  modifier = 0;
  keyCode = 0;

  if (macro.length() == 0) return;

  // Parse modifiers
  if (macro.indexOf("ctrl+") >= 0) { modifier |= 0x01; macro.replace("ctrl+", ""); }
  if (macro.indexOf("shift+") >= 0) { modifier |= 0x02; macro.replace("shift+", ""); }
  if (macro.indexOf("alt+") >= 0) { modifier |= 0x04; macro.replace("alt+", ""); }
  if (macro.indexOf("win+") >= 0) { modifier |= 0x08; macro.replace("win+", ""); } // Added Win key (GUI) support if supported by receiver

  macro.trim();
  
  // Basic Letters (a-z) -> 0x04..0x1D
  if (macro.length() == 1 && macro[0] >= 'a' && macro[0] <= 'z') {
    keyCode = 0x04 + (macro[0] - 'a');
    return;
  }
  
  // Basic Numbers (0-9) -> 0x1E..0x27 (Note: 1 is 0x1E, 0 is 0x27)
  if (macro.length() == 1 && macro[0] >= '0' && macro[0] <= '9') {
     if (macro[0] == '0') keyCode = 0x27;
     else keyCode = 0x1E + (macro[0] - '1');
     return;
  }

  // Special Keys & F-Keys
  if (macro == "enter") keyCode = 0x28;
  else if (macro == "esc" || macro == "escape") keyCode = 0x29;
  else if (macro == "backspace") keyCode = 0x2A;
  else if (macro == "tab") keyCode = 0x2B;
  else if (macro == "space") keyCode = 0x2C;
  else if (macro == "minus" || macro == "-") keyCode = 0x2D;
  else if (macro == "equal" || macro == "=") keyCode = 0x2E;
  else if (macro == "bracketleft" || macro == "[") keyCode = 0x2F;
  else if (macro == "bracketright" || macro == "]") keyCode = 0x30;
  else if (macro == "backslash" || macro == "\\") keyCode = 0x31;
  else if (macro == "semicolon" || macro == ";") keyCode = 0x33;
  else if (macro == "quote" || macro == "'") keyCode = 0x34;
  else if (macro == "backquote" || macro == "`") keyCode = 0x35;
  else if (macro == "comma" || macro == ",") keyCode = 0x36;
  else if (macro == "period" || macro == ".") keyCode = 0x37;
  else if (macro == "slash" || macro == "/") keyCode = 0x38;
  else if (macro == "capslock") keyCode = 0x39;

  // German Umlauts (Mapped to US Layout Scancodes)
  // ä -> ' (0x34)
  else if (macro == "ä" || macro == "ae") keyCode = 0x34;
  // ö -> ; (0x33)
  else if (macro == "ö" || macro == "oe") keyCode = 0x33;
  // ü -> [ (0x2F)
  else if (macro == "ü" || macro == "ue") keyCode = 0x2F;
  // ß -> - (0x2D)
  else if (macro == "ß" || macro == "ss") keyCode = 0x2D;

  // Function Keys F1-F12
  else if (macro == "f1") keyCode = 0x3A;
  else if (macro == "f2") keyCode = 0x3B;
  else if (macro == "f3") keyCode = 0x3C;
  else if (macro == "f4") keyCode = 0x3D;
  else if (macro == "f5") keyCode = 0x3E;
  else if (macro == "f6") keyCode = 0x3F;
  else if (macro == "f7") keyCode = 0x40;
  else if (macro == "f8") keyCode = 0x41;
  else if (macro == "f9") keyCode = 0x42;
  else if (macro == "f10") keyCode = 0x43;
  else if (macro == "f11") keyCode = 0x44;
  else if (macro == "f12") keyCode = 0x45;

  // System Keys
  else if (macro == "printscreen") keyCode = 0x46;
  else if (macro == "scrolllock") keyCode = 0x47;
  else if (macro == "pause") keyCode = 0x48;
  else if (macro == "insert") keyCode = 0x49;
  else if (macro == "home") keyCode = 0x4A;
  else if (macro == "pageup") keyCode = 0x4B;
  else if (macro == "delete") keyCode = 0x4C;
  else if (macro == "end") keyCode = 0x4D;
  else if (macro == "pagedown") keyCode = 0x4E;
  else if (macro == "right") keyCode = 0x4F;
  else if (macro == "left") keyCode = 0x50;
  else if (macro == "down") keyCode = 0x51;
  else if (macro == "up") keyCode = 0x52;

  // Keypad
  else if (macro == "numlock") keyCode = 0x53;
  else if (macro == "divide") keyCode = 0x54;
  else if (macro == "multiply") keyCode = 0x55;
  else if (macro == "subtract") keyCode = 0x56;
  else if (macro == "add") keyCode = 0x57;
  else if (macro == "numenter") keyCode = 0x58;
  else if (macro == "num1") keyCode = 0x59;
  else if (macro == "num2") keyCode = 0x5A;
  else if (macro == "num3") keyCode = 0x5B;
  else if (macro == "num4") keyCode = 0x5C;
  else if (macro == "num5") keyCode = 0x5D;
  else if (macro == "num6") keyCode = 0x5E;
  else if (macro == "num7") keyCode = 0x5F;
  else if (macro == "num8") keyCode = 0x60;
  else if (macro == "num9") keyCode = 0x61;
  else if (macro == "num0") keyCode = 0x62;
  else if (macro == "decimal") keyCode = 0x63;

  // Multimedia
  else if (macro == "mute" || macro == "audiomute") keyCode = 0x7F;
  else if (macro == "volup" || macro == "audiovolup") keyCode = 0x80;
  else if (macro == "voldown" || macro == "audiovoldown") keyCode = 0x81;
}
