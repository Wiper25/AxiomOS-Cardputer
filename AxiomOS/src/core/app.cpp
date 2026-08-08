#include "core/app.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "core/config.h"

namespace axiom {
namespace {

const char* StateName(voice::State s) {
  switch (s) {
    case voice::State::Listen:
      return "LISTEN";
    case voice::State::Thinking:
      return "THINK";
    case voice::State::Speak:
      return "SPEAK";
    default:
      return "IDLE";
  }
}

}  // namespace

bool App::HasCompileWifi() {
  return strcmp(AXIOM_WIFI_SSID, "YOUR_SSID") != 0 && AXIOM_WIFI_SSID[0] != '\0';
}

bool App::WaitWifiConnected(uint32_t timeout_ms) {
  const uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    wifi_.Tick();
    const auto wt = wifi_.GetTelemetry();
    M5.Display.fillRect(0, 40, M5.Display.width(), 40, TFT_BLACK);
    M5.Display.setCursor(4, 44);
    if (wt.connect_state == modules::WifiConnectState::Connecting) {
      M5.Display.printf("connecting %s...\n", wt.connected_ssid);
    } else if (wt.connected) {
      M5.Display.printf("OK %s\n", WiFi.localIP().toString().c_str());
      return true;
    } else if (wt.connect_state == modules::WifiConnectState::Failed) {
      M5.Display.println("wifi failed");
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return wifi_.GetTelemetry().connected;
}

void App::EnterWifiSetup() {
  ui_ = UiMode::WifiScan;
  wifi_sel_ = 0;
  pass_len_ = 0;
  pass_buf_[0] = 0;
  join_ssid_[0] = 0;
  keyboard_.SetTextCapture(false);
  wifi_.SetScannerActive(true);
  wifi_.StartScan();
  wifi_ui_ms_ = 0;
  last_status_key_[0] = 0;
  DrawWifiUi(true);
}

void App::ExitWifiSetup() {
  ui_ = UiMode::Voice;
  keyboard_.SetTextCapture(false);
  wifi_.SetScannerActive(false);
  pass_len_ = 0;
  pass_buf_[0] = 0;
  last_status_key_[0] = 0;
  DrawStatus(true);
}

void App::DrawWifiUi(bool force) {
  const auto wt = wifi_.GetTelemetry();
  char key[128];
  snprintf(key, sizeof(key), "%d|%d|%d|%u|%s|%u", static_cast<int>(ui_), wt.scanning ? 1 : 0,
           wt.networks_found, wifi_sel_, pass_buf_,
           static_cast<unsigned>(wt.connect_state));
  if (!force && strcmp(key, last_status_key_) == 0) return;
  strncpy(last_status_key_, key, sizeof(last_status_key_) - 1);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(4, 2);
  M5.Display.println("WiFi setup");

  if (ui_ == UiMode::WifiConnecting) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.printf("\nConnecting\n%s\n", join_ssid_);
    if (wt.connect_state == modules::WifiConnectState::Failed) {
      M5.Display.setTextColor(TFT_RED, TFT_BLACK);
      M5.Display.println("FAILED — Enter retry");
    } else if (wt.connected) {
      M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
      M5.Display.printf("OK %s\n", WiFi.localIP().toString().c_str());
    }
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.println("\n` back");
    return;
  }

  if (ui_ == UiMode::WifiPass) {
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.printf("\nSSID %s\n", join_ssid_);
    M5.Display.println("Password:");
    M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    M5.Display.printf("%s_\n", pass_buf_);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.println("\nEnter=connect  `=back");
    M5.Display.println("Del=erase");
    return;
  }

  // WifiScan
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  if (wt.scanning) {
    M5.Display.println("\nScanning...");
  } else if (wifi_.NetworkCount() == 0) {
    M5.Display.println("\nNo networks");
    M5.Display.println("R = rescan");
  } else {
    M5.Display.println("\n;/. select  Enter join");
    const uint8_t n = wifi_.NetworkCount();
    const uint8_t start = wifi_sel_ > 2 ? static_cast<uint8_t>(wifi_sel_ - 2) : 0;
    for (uint8_t i = start; i < n && i < start + 5; ++i) {
      const auto& net = wifi_.NetworkAt(i);
      M5.Display.setTextColor(i == wifi_sel_ ? TFT_GREENYELLOW : TFT_WHITE, TFT_BLACK);
      M5.Display.printf("%c%s %ddB%s\n", i == wifi_sel_ ? '>' : ' ', net.ssid,
                        static_cast<int>(net.rssi), net.encrypted ? "*" : "");
    }
  }
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.println("R rescan  ` exit");
  if (wifi_.HasSaved()) {
    M5.Display.printf("saved:%s\n", wifi_.SavedSsid());
  }
}

void App::HandleWifiAction(drivers::InputAction action) {
  using drivers::InputAction;
  const auto wt = wifi_.GetTelemetry();

  if (action == InputAction::Back) {
    if (ui_ == UiMode::WifiPass) {
      ui_ = UiMode::WifiScan;
      keyboard_.SetTextCapture(false);
      pass_len_ = 0;
      pass_buf_[0] = 0;
      last_status_key_[0] = 0;
      DrawWifiUi(true);
      return;
    }
    ExitWifiSetup();
    return;
  }

  if (ui_ == UiMode::WifiConnecting) {
    if (action == InputAction::Select) {
      if (wt.connected) {
        ExitWifiSetup();
      } else {
        wifi_.Connect(join_ssid_, pass_buf_);
        ui_ = UiMode::WifiConnecting;
        last_status_key_[0] = 0;
      }
    }
    return;
  }

  if (ui_ == UiMode::WifiPass) {
    if (action == InputAction::Char) {
      const char c = keyboard_.LastChar();
      if (c && pass_len_ + 1 < sizeof(pass_buf_)) {
        pass_buf_[pass_len_++] = c;
        pass_buf_[pass_len_] = 0;
        last_status_key_[0] = 0;
      }
    } else if (action == InputAction::DeleteChar) {
      if (pass_len_ > 0) {
        pass_buf_[--pass_len_] = 0;
        last_status_key_[0] = 0;
      }
    } else if (action == InputAction::Select) {
      wifi_.Connect(join_ssid_, pass_buf_);
      ui_ = UiMode::WifiConnecting;
      keyboard_.SetTextCapture(false);
      last_status_key_[0] = 0;
      DrawWifiUi(true);
    }
    return;
  }

  // WifiScan
  if (action == InputAction::Rescan || action == InputAction::QuickWireless) {
    wifi_sel_ = 0;
    wifi_.StartScan();
    last_status_key_[0] = 0;
    return;
  }
  if (action == InputAction::Up) {
    if (wifi_sel_ > 0) --wifi_sel_;
    last_status_key_[0] = 0;
    return;
  }
  if (action == InputAction::Down) {
    const uint8_t n = wifi_.NetworkCount();
    if (n > 0 && wifi_sel_ + 1 < n) ++wifi_sel_;
    last_status_key_[0] = 0;
    return;
  }
  if (action == InputAction::Select) {
    if (wifi_.NetworkCount() == 0) {
      wifi_.StartScan();
      return;
    }
    const auto& net = wifi_.NetworkAt(wifi_sel_);
    strncpy(join_ssid_, net.ssid, sizeof(join_ssid_) - 1);
    join_ssid_[sizeof(join_ssid_) - 1] = 0;
    if (!net.encrypted) {
      pass_len_ = 0;
      pass_buf_[0] = 0;
      wifi_.Connect(join_ssid_, "");
      ui_ = UiMode::WifiConnecting;
      last_status_key_[0] = 0;
      DrawWifiUi(true);
      return;
    }
    pass_len_ = 0;
    pass_buf_[0] = 0;
    ui_ = UiMode::WifiPass;
    keyboard_.SetTextCapture(true);
    last_status_key_[0] = 0;
    DrawWifiUi(true);
  }
}

bool App::Begin() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(180);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, 4);
  M5.Display.println("AxiomOS Voice");
  M5.Display.println("booting...");

  if (!keyboard_.Begin()) return false;
  if (!audio_.Begin()) return false;
  audio_.SetVolume(200);

  wifi_.Begin();  // NVS auto-connect
  M5.Display.println("wifi...");

  if (!wifi_.GetTelemetry().connected && HasCompileWifi()) {
    M5.Display.printf("try %s\n", AXIOM_WIFI_SSID);
    wifi_.Connect(AXIOM_WIFI_SSID, AXIOM_WIFI_PASS);
  }

  if (!wifi_.GetTelemetry().connected) {
    if (wifi_.GetTelemetry().connect_state == modules::WifiConnectState::Connecting ||
        wifi_.HasSaved() || HasCompileWifi()) {
      WaitWifiConnected(12000);
    }
  }

  if (!wifi_.GetTelemetry().connected) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.println("WiFi needed — setup");
    vTaskDelay(pdMS_TO_TICKS(400));
    EnterWifiSetup();
  }

  voice::VoiceConfig vcfg;
  strncpy(vcfg.host, AXIOM_VOICE_HOST, sizeof(vcfg.host) - 1);
  vcfg.port = static_cast<uint16_t>(AXIOM_VOICE_PORT);
  strncpy(vcfg.path, AXIOM_VOICE_PATH, sizeof(vcfg.path) - 1);
  vcfg.enabled = true;
  if (!voice_.Begin(vcfg)) return false;
  voice_.SetAudioDriver(&audio_);
  voice_.SetEnabled(true);

  audio_.Play(drivers::SoundId::BootMelody);
  if (ui_ == UiMode::Voice) DrawStatus(true);
  return true;
}

void App::DrawStatus(bool force) {
  if (ui_ != UiMode::Voice) {
    DrawWifiUi(force);
    return;
  }

  const auto wt = wifi_.GetTelemetry();
  const auto vt = voice_.GetTelemetry();

  char key[96];
  snprintf(key, sizeof(key), "%d|%s|%d|%s|%s|%s",
           wt.connected ? 1 : 0, wt.connected_ssid, static_cast<int>(vt.state), vt.status,
           vt.ws_connected ? "ws" : "-", WiFi.localIP().toString().c_str());
  if (!force && strcmp(key, last_status_key_) == 0) return;
  strncpy(last_status_key_, key, sizeof(last_status_key_) - 1);

  if (vt.state == voice::State::Speak && !force) {
    M5.Display.fillRect(0, 70, M5.Display.width(), 50, TFT_BLACK);
    M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    M5.Display.setCursor(4, 72);
    M5.Display.printf("STATE %s\n", StateName(vt.state));
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.printf("%s\n", vt.status);
    M5.Display.printf("ws %s  tx%lu rx%lu\n", vt.ws_connected ? "ON" : "off",
                      static_cast<unsigned long>(vt.tx_chunks),
                      static_cast<unsigned long>(vt.rx_chunks));
    return;
  }

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.printf("AxiomOS Voice %s\n", kProjectVersion);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.printf("WiFi %s\n", wt.connected ? "OK" : "WAIT");
  if (wt.connected) {
    M5.Display.printf("%s\n", wt.connected_ssid);
    M5.Display.printf("%s\n", WiFi.localIP().toString().c_str());
    M5.Display.printf("rssi %d\n", static_cast<int>(wt.link_rssi));
  } else {
    M5.Display.println("press W = WiFi");
  }

  M5.Display.printf("\nSRV %s:%u\n", AXIOM_VOICE_HOST, AXIOM_VOICE_PORT);
  M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  M5.Display.printf("STATE %s\n", StateName(vt.state));
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.printf("%s\n", vt.status);
  M5.Display.printf("ws %s  tx%lu rx%lu\n", vt.ws_connected ? "ON" : "off",
                    static_cast<unsigned long>(vt.tx_chunks),
                    static_cast<unsigned long>(vt.rx_chunks));
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.println("\nHold V=talk  W=WiFi");
}

void App::Loop() {
  M5Cardputer.update();
  wifi_.Tick();

  // Auto-leave connecting screen on success
  if (ui_ == UiMode::WifiConnecting && wifi_.GetTelemetry().connected) {
    audio_.Play(drivers::SoundId::Success);
    ExitWifiSetup();
  }

  drivers::InputAction action = drivers::InputAction::None;
  if (keyboard_.Poll(action)) {
    if (ui_ != UiMode::Voice) {
      HandleWifiAction(action);
    } else if (action == drivers::InputAction::QuickWireless ||
               action == drivers::InputAction::Rescan) {
      // W / 1 / R from voice screen → WiFi (R only if idle)
      if (voice_.GetState() == voice::State::Idle) {
        EnterWifiSetup();
      }
    } else if (action == drivers::InputAction::VoicePtt) {
      if (wifi_.GetTelemetry().connected) {
        audio_.SetExclusive(false);
        voice_.PttDown();
      } else {
        EnterWifiSetup();
      }
    } else if (action == drivers::InputAction::VoicePttRelease) {
      voice_.PttUp();
    }
  }

  audio_.Tick();
  if (ui_ == UiMode::Voice) {
    voice_.Tick();
    audio_.SetExclusive(voice_.IsAudioExclusive());
  } else {
    audio_.SetExclusive(false);
  }

  const uint32_t now = millis();
  const uint32_t redraw_ms =
      (ui_ == UiMode::Voice && voice_.GetState() == voice::State::Speak) ? 500
                                                                         : kStatusRedrawMs;
  if (now - last_status_ms_ >= redraw_ms) {
    last_status_ms_ = now;
    if (ui_ == UiMode::Voice) {
      DrawStatus(false);
    } else {
      DrawWifiUi(false);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}

}  // namespace axiom
