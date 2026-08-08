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

void HintBar(const char* line) {
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.println(line);
}

}  // namespace

uint8_t App::WifiListRows() const {
  // networks + last row "[ввести SSID]"
  return static_cast<uint8_t>(wifi_.NetworkCount() + 1);
}

void App::EnterWifiSetup() {
  ui_ = UiMode::WifiScan;
  wifi_sel_ = 0;
  pass_len_ = 0;
  pass_buf_[0] = 0;
  ssid_len_ = 0;
  ssid_buf_[0] = 0;
  join_ssid_[0] = 0;
  keyboard_.SetTextCapture(false);
  wifi_.Disconnect();  // free radio for scan
  wifi_.SetScannerActive(true);
  wifi_.StartScan();
  last_status_key_[0] = 0;
  DrawWifiUi(true);
}

void App::ExitWifiSetup() {
  ui_ = UiMode::Voice;
  keyboard_.SetTextCapture(false);
  wifi_.SetScannerActive(false);
  pass_len_ = 0;
  pass_buf_[0] = 0;
  ssid_len_ = 0;
  ssid_buf_[0] = 0;
  last_status_key_[0] = 0;
  DrawStatus(true);
}

void App::StartJoinSelected() {
  const uint8_t n = wifi_.NetworkCount();
  if (wifi_sel_ >= n) {
    // manual SSID row
    ssid_len_ = 0;
    ssid_buf_[0] = 0;
    ui_ = UiMode::WifiSsid;
    keyboard_.SetTextCapture(true);
    last_status_key_[0] = 0;
    DrawWifiUi(true);
    return;
  }
  const auto& net = wifi_.NetworkAt(wifi_sel_);
  strncpy(join_ssid_, net.ssid, sizeof(join_ssid_) - 1);
  join_ssid_[sizeof(join_ssid_) - 1] = 0;
  pass_len_ = 0;
  pass_buf_[0] = 0;
  if (!net.encrypted) {
    wifi_.Connect(join_ssid_, "");
    ui_ = UiMode::WifiConnecting;
    keyboard_.SetTextCapture(false);
  } else {
    ui_ = UiMode::WifiPass;
    keyboard_.SetTextCapture(true);
  }
  last_status_key_[0] = 0;
  DrawWifiUi(true);
}

void App::StartJoinManual() {
  if (ssid_len_ == 0) return;
  strncpy(join_ssid_, ssid_buf_, sizeof(join_ssid_) - 1);
  join_ssid_[sizeof(join_ssid_) - 1] = 0;
  pass_len_ = 0;
  pass_buf_[0] = 0;
  ui_ = UiMode::WifiPass;
  keyboard_.SetTextCapture(true);
  last_status_key_[0] = 0;
  DrawWifiUi(true);
}

void App::DrawWifiUi(bool force) {
  const auto wt = wifi_.GetTelemetry();
  char key[160];
  snprintf(key, sizeof(key), "%d|%d|%d|%u|%s|%s|%u", static_cast<int>(ui_),
           wt.scanning ? 1 : 0, wt.networks_found, wifi_sel_, ssid_buf_, pass_buf_,
           static_cast<unsigned>(wt.connect_state));
  if (!force && strcmp(key, last_status_key_) == 0) return;
  strncpy(last_status_key_, key, sizeof(last_status_key_) - 1);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, 2);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.println("=== Wi-Fi ===");

  if (ui_ == UiMode::WifiConnecting) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.printf("\nПодключение...\n%s\n", join_ssid_);
    if (wt.connect_state == modules::WifiConnectState::Failed) {
      M5.Display.setTextColor(TFT_RED, TFT_BLACK);
      M5.Display.println("\nОШИБКА");
      HintBar("Enter=ещё  `=назад");
    } else if (wt.connected) {
      M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
      M5.Display.printf("\nOK\n%s\n", WiFi.localIP().toString().c_str());
      HintBar("Enter=голос");
    } else {
      HintBar("жди...  `=отмена");
    }
    return;
  }

  if (ui_ == UiMode::WifiSsid) {
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.println("\nИмя сети (SSID):");
    M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    M5.Display.printf("%s_\n", ssid_buf_);
    HintBar("\nEnter=далее  Del=стёр  `=назад");
    return;
  }

  if (ui_ == UiMode::WifiPass) {
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.printf("\nСеть: %s\n", join_ssid_);
    M5.Display.println("Пароль:");
    M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    M5.Display.printf("%s_\n", pass_buf_);
    HintBar("\nEnter=подключить");
    HintBar("Del=стёр  `=назад");
    return;
  }

  // WifiScan
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  if (wt.scanning) {
    M5.Display.println("\nПоиск сетей...");
    HintBar("подожди");
  } else {
    const uint8_t rows = WifiListRows();
    M5.Display.println(";/. выбор  Enter OK");
    if (rows == 1 && wifi_.NetworkCount() == 0) {
      M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
      M5.Display.println("сетей нет — R");
    }
    // scroll window
    const uint8_t win = 5;
    uint8_t start = 0;
    if (wifi_sel_ >= win) start = static_cast<uint8_t>(wifi_sel_ - win + 1);
    for (uint8_t i = start; i < rows && i < start + win; ++i) {
      const bool sel = (i == wifi_sel_);
      M5.Display.setTextColor(sel ? TFT_BLACK : TFT_WHITE, sel ? TFT_GREENYELLOW : TFT_BLACK);
      if (i < wifi_.NetworkCount()) {
        const auto& net = wifi_.NetworkAt(i);
        char line[40];
        snprintf(line, sizeof(line), "%c%-16s%4d%s", sel ? '>' : ' ', net.ssid,
                 static_cast<int>(net.rssi), net.encrypted ? "*" : " ");
        M5.Display.println(line);
      } else {
        M5.Display.println(sel ? "> [ввести SSID]" : "  [ввести SSID]");
      }
    }
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.println("R=скан  `=выход");
    if (wifi_.HasSaved()) {
      M5.Display.printf("NVS:%s\n", wifi_.SavedSsid());
    }
  }
}

void App::HandleWifiAction(drivers::InputAction action) {
  using drivers::InputAction;

  if (action == InputAction::Back) {
    if (ui_ == UiMode::WifiPass) {
      // back to scan or ssid entry
      if (ssid_len_ > 0 && strcmp(join_ssid_, ssid_buf_) == 0) {
        ui_ = UiMode::WifiSsid;
        keyboard_.SetTextCapture(true);
      } else {
        ui_ = UiMode::WifiScan;
        keyboard_.SetTextCapture(false);
      }
      pass_len_ = 0;
      pass_buf_[0] = 0;
      last_status_key_[0] = 0;
      DrawWifiUi(true);
      return;
    }
    if (ui_ == UiMode::WifiSsid) {
      ui_ = UiMode::WifiScan;
      keyboard_.SetTextCapture(false);
      ssid_len_ = 0;
      ssid_buf_[0] = 0;
      last_status_key_[0] = 0;
      DrawWifiUi(true);
      return;
    }
    if (ui_ == UiMode::WifiConnecting) {
      wifi_.Disconnect();
      ui_ = UiMode::WifiScan;
      keyboard_.SetTextCapture(false);
      wifi_.StartScan();
      last_status_key_[0] = 0;
      DrawWifiUi(true);
      return;
    }
    // stay in setup if still offline — only exit when connected or user insists twice?
    if (wifi_.GetTelemetry().connected) {
      ExitWifiSetup();
    } else {
      // allow exit to voice screen anyway (can reopen with W)
      ExitWifiSetup();
    }
    return;
  }

  if (ui_ == UiMode::WifiConnecting) {
    if (action == InputAction::Select) {
      if (wifi_.GetTelemetry().connected) {
        ExitWifiSetup();
      } else if (wifi_.GetTelemetry().connect_state == modules::WifiConnectState::Failed) {
        wifi_.Connect(join_ssid_, pass_buf_);
        last_status_key_[0] = 0;
      }
    }
    return;
  }

  if (ui_ == UiMode::WifiSsid) {
    if (action == InputAction::Char) {
      const char c = keyboard_.LastChar();
      if (c && ssid_len_ + 1 < sizeof(ssid_buf_)) {
        ssid_buf_[ssid_len_++] = c;
        ssid_buf_[ssid_len_] = 0;
        last_status_key_[0] = 0;
      }
    } else if (action == InputAction::DeleteChar) {
      if (ssid_len_ > 0) {
        ssid_buf_[--ssid_len_] = 0;
        last_status_key_[0] = 0;
      }
    } else if (action == InputAction::Select) {
      StartJoinManual();
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
    const uint8_t rows = WifiListRows();
    if (rows > 0 && wifi_sel_ + 1 < rows) ++wifi_sel_;
    last_status_key_[0] = 0;
    return;
  }
  if (action == InputAction::Select) {
    StartJoinSelected();
  }
}

bool App::Begin() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(200);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, 4);
  M5.Display.println("AxiomOS Voice");
  M5.Display.println("boot...");

  if (!keyboard_.Begin()) return false;
  if (!audio_.Begin()) return false;
  audio_.SetVolume(200);

  wifi_.Begin();  // tries NVS auto-connect (non-blocking start)

  // Brief wait only if NVS/compile creds already kicking
  const bool try_known =
      wifi_.HasSaved() ||
      (strcmp(AXIOM_WIFI_SSID, "YOUR_SSID") != 0 && AXIOM_WIFI_SSID[0] != '\0');
  if (try_known && !wifi_.GetTelemetry().connected) {
    if (!wifi_.HasSaved() && strcmp(AXIOM_WIFI_SSID, "YOUR_SSID") != 0) {
      wifi_.Connect(AXIOM_WIFI_SSID, AXIOM_WIFI_PASS);
    }
    const uint32_t t0 = millis();
    while (millis() - t0 < 6000) {
      wifi_.Tick();
      if (wifi_.GetTelemetry().connected) break;
      if (wifi_.GetTelemetry().connect_state == modules::WifiConnectState::Failed) break;
      M5.Display.fillRect(0, 24, M5.Display.width(), 16, TFT_BLACK);
      M5.Display.setCursor(4, 24);
      M5.Display.printf("wifi %s...", wifi_.GetTelemetry().connected_ssid);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  voice::VoiceConfig vcfg;
  strncpy(vcfg.host, AXIOM_VOICE_HOST, sizeof(vcfg.host) - 1);
  vcfg.port = static_cast<uint16_t>(AXIOM_VOICE_PORT);
  strncpy(vcfg.path, AXIOM_VOICE_PATH, sizeof(vcfg.path) - 1);
  vcfg.enabled = true;
  if (!voice_.Begin(vcfg)) return false;
  voice_.SetAudioDriver(&audio_);
  voice_.SetEnabled(true);

  if (!wifi_.GetTelemetry().connected) {
    // Main path: interactive UI on device
    EnterWifiSetup();
  } else {
    audio_.Play(drivers::SoundId::BootMelody);
    DrawStatus(true);
  }
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
    return;
  }

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.printf("AxiomOS %s\n", kProjectVersion);

  M5.Display.setTextColor(wt.connected ? TFT_GREEN : TFT_RED, TFT_BLACK);
  M5.Display.printf("WiFi %s\n", wt.connected ? "OK" : "OFF");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  if (wt.connected) {
    M5.Display.printf("%s\n%s\n", wt.connected_ssid, WiFi.localIP().toString().c_str());
  } else {
    M5.Display.println(">>> W = Wi-Fi <<<");
  }

  M5.Display.printf("\nSRV %s:%u\n", AXIOM_VOICE_HOST, AXIOM_VOICE_PORT);
  M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  M5.Display.printf("STATE %s\n", StateName(vt.state));
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.printf("%s\n", vt.status);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.println("\nV=говор  W=Wi-Fi");
}

void App::Loop() {
  M5Cardputer.update();
  wifi_.Tick();

  if (ui_ == UiMode::WifiConnecting && wifi_.GetTelemetry().connected) {
    audio_.Play(drivers::SoundId::Success);
    vTaskDelay(pdMS_TO_TICKS(200));
    ExitWifiSetup();
    audio_.Play(drivers::SoundId::BootMelody);
  }

  drivers::InputAction action = drivers::InputAction::None;
  if (keyboard_.Poll(action)) {
    if (ui_ != UiMode::Voice) {
      HandleWifiAction(action);
    } else if (action == drivers::InputAction::QuickWireless) {
      if (voice_.GetState() == voice::State::Idle) EnterWifiSetup();
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
    if (ui_ == UiMode::Voice)
      DrawStatus(false);
    else
      DrawWifiUi(false);
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}

}  // namespace axiom
