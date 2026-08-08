#pragma once

#include "drivers/audio/audio_driver.h"
#include "drivers/keyboard/keyboard_driver.h"
#include "modules/voice/voice_assistant.h"
#include "modules/wifi/wifi_module.h"

namespace axiom {

// On-device WiFi UI (scan → optional SSID type → password → connect → NVS)
enum class UiMode : uint8_t {
  Voice = 0,
  WifiScan,
  WifiSsid,       // manual SSID entry
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
  void StartJoinSelected();
  void StartJoinManual();
  uint8_t WifiListRows() const;  // networks + 1 "type SSID" row

  drivers::KeyboardDriver keyboard_;
  drivers::AudioDriver audio_;
  modules::WifiModule wifi_;
  voice::VoiceAssistant voice_;

  UiMode ui_ = UiMode::Voice;
  uint8_t wifi_sel_ = 0;
  char join_ssid_[33] = {0};
  char pass_buf_[65] = {0};
  uint8_t pass_len_ = 0;
  char ssid_buf_[33] = {0};
  uint8_t ssid_len_ = 0;

  uint32_t last_status_ms_ = 0;
  char last_status_key_[128] = {0};
};

}  // namespace axiom
