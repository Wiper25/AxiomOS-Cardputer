#pragma once

#include "drivers/audio/audio_driver.h"
#include "drivers/keyboard/keyboard_driver.h"
#include "modules/voice/voice_assistant.h"
#include "modules/wifi/wifi_module.h"

namespace axiom {

class App {
 public:
  bool Begin();
  void Loop();

 private:
  void DrawStatus(bool force = false);

  drivers::KeyboardDriver keyboard_;
  drivers::AudioDriver audio_;
  modules::WifiModule wifi_;
  voice::VoiceAssistant voice_;

  uint32_t last_status_ms_ = 0;
  char last_status_key_[96] = {0};
};

}  // namespace axiom
