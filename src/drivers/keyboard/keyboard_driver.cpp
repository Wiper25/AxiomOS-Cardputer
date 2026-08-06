#include "drivers/keyboard/keyboard_driver.h"

#include <Arduino.h>
#include <M5Cardputer.h>

namespace axiom::drivers {

bool KeyboardDriver::Begin() { return true; }

bool KeyboardDriver::Poll(InputAction& action) {
  action = InputAction::None;
  last_char_ = 0;
  const uint32_t now = millis();

  if (M5Cardputer.BtnA.wasPressed() || M5.BtnA.wasPressed()) {
    action = InputAction::Select;
    return true;
  }

  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    return false;
  }
  if (now - last_event_ms_ < 80) {
    return false;
  }

  const auto& ks = M5Cardputer.Keyboard.keysState();

  if (ks.enter) {
    action = InputAction::Select;
    last_event_ms_ = now;
    return true;
  }

  if (ks.del) {
    action = text_capture_ ? InputAction::DeleteChar : InputAction::Back;
    last_event_ms_ = now;
    return true;
  }

  if (text_capture_) {
    for (char c : ks.word) {
      if (c == '`' || c == '~') {
        action = InputAction::Back;
        last_event_ms_ = now;
        return true;
      }
      if (c >= 32 && c <= 126) {
        action = InputAction::Char;
        last_char_ = c;
        last_event_ms_ = now;
        return true;
      }
    }
    return false;
  }

  // Cardputer "стрелки": ; вверх, . вниз
  for (char c : ks.word) {
    switch (c) {
      case ';':
      case ':':
        action = InputAction::Up;
        last_event_ms_ = now;
        return true;
      case '.':
      case '>':
        action = InputAction::Down;
        last_event_ms_ = now;
        return true;
      case '`':
      case '~':
      case ',':
      case '<':
        action = InputAction::Back;
        last_event_ms_ = now;
        return true;
      case 'r':
      case 'R':
        action = InputAction::Rescan;
        last_event_ms_ = now;
        return true;
      case '1':
        action = InputAction::QuickWireless;
        last_event_ms_ = now;
        return true;
      case '2':
        action = InputAction::QuickNetwork;
        last_event_ms_ = now;
        return true;
      case '3':
        action = InputAction::QuickHardware;
        last_event_ms_ = now;
        return true;
      case '4':
        action = InputAction::QuickSystem;
        last_event_ms_ = now;
        return true;
      default:
        break;
    }
  }

  return false;
}

}  // namespace axiom::drivers
