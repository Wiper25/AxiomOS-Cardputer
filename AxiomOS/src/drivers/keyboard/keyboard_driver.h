#pragma once

#include <stdint.h>

namespace axiom::drivers {

enum class InputAction {
  None = 0,
  Up,
  Down,
  Select,
  Back,
  Char,
  DeleteChar,
  Rescan,
  VoicePtt,         // V / Fn+V pressed — start PTT
  VoicePttRelease,  // V released — hangover then end
  QuickWireless,
  QuickNetwork,
  QuickHardware,
  QuickSystem
};

class KeyboardDriver {
 public:
  bool Begin();
  void SetTextCapture(bool enabled) { text_capture_ = enabled; }
  bool Poll(InputAction& action);
  char LastChar() const { return last_char_; }

 private:
  bool IsVoiceKeyDown() const;

  uint32_t last_event_ms_ = 0;
  bool text_capture_ = false;
  bool voice_ptt_held_ = false;
  char last_char_ = 0;
};

}  // namespace axiom::drivers
