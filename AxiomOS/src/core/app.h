#pragma once

#include "drivers/audio/audio_driver.h"
#include "drivers/keyboard/keyboard_driver.h"
#include "modules/voice/voice_assistant.h"
#include "modules/wifi/wifi_module.h"

namespace axiom {

enum class UiMode : uint8_t {
  Voice = 0,
  WifiScan,
  WifiPass,
  WifiConnecting
};

class App {
 public:
  bool Begin();
  void Loop();

 private:
  void DrawStatus(bool force = false);
  void DrawWifiUi(bool force = false);
  void EnterWifiSetup();
  void ExitWifiSetup();
  void HandleWifiAction(drivers::InputAction action);
  bool WaitWifiConnected(uint32_t timeout_ms);
  static bool HasCompileWifi();

  drivers::KeyboardDriver keyboard_;
  drivers::AudioDriver audio_;
  modules::WifiModule wifi_;
  voice::VoiceAssistant voice_;

  UiMode ui_ = UiMode::Voice;
  uint8_t wifi_sel_ = 0;
  char join_ssid_[33] = {0};
  char pass_buf_[65] = {0};
  uint8_t pass_len_ = 0;
  uint32_t wifi_ui_ms_ = 0;

  uint32_t last_status_ms_ = 0;
  char last_status_key_[96] = {0};
};

}  // namespace axiom
