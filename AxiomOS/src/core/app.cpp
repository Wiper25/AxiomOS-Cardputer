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

  wifi_.Begin();
  M5.Display.println("wifi...");
  if (strcmp(AXIOM_WIFI_SSID, "YOUR_SSID") == 0) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.println("EDIT WIFI in config.h");
  } else {
    wifi_.Connect(AXIOM_WIFI_SSID, AXIOM_WIFI_PASS);
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
  DrawStatus(true);
  return true;
}

void App::DrawStatus(bool force) {
  const auto wt = wifi_.GetTelemetry();
  const auto vt = voice_.GetTelemetry();

  char key[96];
  snprintf(key, sizeof(key), "%d|%s|%d|%s|%s|%s",
           wt.connected ? 1 : 0, wt.connected_ssid, static_cast<int>(vt.state), vt.status,
           vt.ws_connected ? "ws" : "-", WiFi.localIP().toString().c_str());
  if (!force && strcmp(key, last_status_key_) == 0) return;
  strncpy(last_status_key_, key, sizeof(last_status_key_) - 1);

  // During Speak: never fillScreen — SPI display starves I2S / WS loop → mute TTS
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
    M5.Display.printf("ssid %s\n", AXIOM_WIFI_SSID);
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
  M5.Display.println("\nHold V = talk");
}

void App::Loop() {
  M5Cardputer.update();
  wifi_.Tick();

  drivers::InputAction action = drivers::InputAction::None;
  if (keyboard_.Poll(action)) {
    if (action == drivers::InputAction::VoicePtt) {
      audio_.SetExclusive(false);
      voice_.PttDown();
    } else if (action == drivers::InputAction::VoicePttRelease) {
      voice_.PttUp();
    }
  }

  audio_.Tick();
  // Voice first — don't let UI redraw delay WS/PCM
  voice_.Tick();
  audio_.SetExclusive(voice_.IsAudioExclusive());

  const uint32_t now = millis();
  const uint32_t redraw_ms =
      (voice_.GetState() == voice::State::Speak) ? 500 : kStatusRedrawMs;
  if (now - last_status_ms_ >= redraw_ms) {
    last_status_ms_ = now;
    DrawStatus(false);
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}

}  // namespace axiom
