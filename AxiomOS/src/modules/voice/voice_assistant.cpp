#include "modules/voice/voice_assistant.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

#include "drivers/audio/audio_driver.h"

namespace axiom::voice {

bool VoiceAssistant::Begin(const VoiceConfig& cfg) {
  if (began_) return true;
  cfg_ = cfg;
  if (cfg_.host[0] == 0) {
    strncpy(cfg_.host, kDefaultWsHost, sizeof(cfg_.host) - 1);
  }
  if (cfg_.path[0] == 0) {
    strncpy(cfg_.path, kDefaultWsPath, sizeof(cfg_.path) - 1);
  }
  if (cfg_.port == 0) cfg_.port = kDefaultWsPort;

  ws_.Begin();
  tx_queue_ = xQueueCreate(kTxQueueDepth, kChunkBytes);
  if (tx_queue_ == nullptr) return false;

  audio_io_.Begin();
  began_ = true;
  enabled_ = cfg_.enabled;
  SetStatus(enabled_ ? "voice idle" : "voice off");

  xTaskCreatePinnedToCore(MicTaskEntry, "voice_mic", kMicTaskStackWords, this, kMicTaskPriority,
                          &mic_task_, 1);
  xTaskCreatePinnedToCore(SpkTaskEntry, "voice_spk", kSpkTaskStackWords, this, kSpkTaskPriority,
                          &spk_task_, 1);
  return true;
}

void VoiceAssistant::SetEnabled(bool v) {
  enabled_ = v;
  cfg_.enabled = v;
  if (!v) {
    cancel_req_ = true;
    audio_io_.MicStop();
  }
  if (audio_) audio_->SetExclusive(false);
  SetStatus(v ? "voice idle" : "voice off");
}

void VoiceAssistant::SetStatus(const char* s) {
  if (!s) return;
  strncpy(status_, s, sizeof(status_) - 1);
  status_[sizeof(status_) - 1] = 0;
}

VoiceTelemetry VoiceAssistant::GetTelemetry() const {
  VoiceTelemetry t;
  t.state = state_;
  t.enabled = enabled_;
  t.ws_connected = ws_.IsConnected();
  t.audio_exclusive = IsAudioExclusive();
  t.vad_level = vad_.LastLevel();
  t.tx_chunks = tx_chunks_;
  t.rx_chunks = rx_chunks_;
  strncpy(t.status, status_, sizeof(t.status) - 1);
  return t;
}

void VoiceAssistant::ForceListen() {
  if (!enabled_ || !began_) return;
  ptt_session_ = false;
  ptt_held_ = false;
  ptt_release_ms_ = 0;
  force_listen_ = true;
}

void VoiceAssistant::PttDown() {
  if (!began_) return;
  if (!enabled_) SetEnabled(true);
  ptt_held_ = true;
  ptt_release_ms_ = 0;
  ptt_session_ = true;
  if (state_ == State::Listen) {
    SetStatus("listening");
    return;
  }
  if (state_ == State::Thinking || state_ == State::Speak) {
    cancel_req_ = true;
    force_listen_ = true;
    return;
  }
  force_listen_ = true;
}

void VoiceAssistant::PttUp() {
  ptt_held_ = false;
  if (!ptt_session_) return;
  if (state_ == State::Listen || force_listen_) {
    ptt_release_ms_ = millis();
    SetStatus("hang +2s");
  }
}

void VoiceAssistant::FinishUtterance() {
  if (state_ == State::Listen) {
    ptt_held_ = false;
    ptt_release_ms_ = 0;
    mic_run_ = false;
    SetStatus("ending...");
  }
}

void VoiceAssistant::Cancel() { cancel_req_ = true; }

void VoiceAssistant::FlushTxQueue() {
  if (!tx_queue_) return;
  uint8_t dump[kChunkBytes];
  while (xQueueReceive(tx_queue_, dump, 0) == pdTRUE) {
  }
}

bool VoiceAssistant::EnsureWs() {
  if (ws_.IsConnected()) return true;
  if (WiFi.status() != WL_CONNECTED) {
    SetStatus("need WiFi");
    return false;
  }
  SetStatus("ws connect...");
  if (!ws_.Connect(cfg_.host, cfg_.port, cfg_.path)) return false;
  const uint32_t deadline = millis() + 4000;
  while (millis() < deadline) {
    ws_.Tick();
    if (ws_.IsConnected()) return true;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  SetStatus("ws timeout");
  ws_.Disconnect();
  return false;
}

void VoiceAssistant::EnterIdle() {
  mic_run_ = false;
  spk_run_ = false;
  FlushTxQueue();
  vad_.Reset();
  cancel_req_ = false;
  wake_req_ = false;
  ptt_held_ = false;
  ptt_release_ms_ = 0;
  if (!force_listen_) ptt_session_ = false;
  // Must end Speaker too — otherwise 2nd Listen gets silent mic (peak~30)
  audio_io_.ReleaseBus();
  if (state_ != State::Idle || ws_.IsConnected()) {
    ws_.Disconnect();
  }
  state_ = State::Idle;
  if (audio_) audio_->SetExclusive(false);
  SetStatus(enabled_ ? "voice idle" : "voice off");
}

void VoiceAssistant::EnterListen(bool from_vad) {
  (void)from_vad;
  if (!EnsureWs()) {
    if (audio_) audio_->Play(drivers::SoundId::Error);
    EnterIdle();
    return;
  }
  FlushTxQueue();
  vad_.Reset();
  // Beep BEFORE mic — never tone() while Mic owns ES8311
  if (audio_) {
    audio_->SetExclusive(false);
    audio_->Play(drivers::SoundId::Success);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (!ws_.SendTxt("{\"event\":\"listening\"}")) {
    SetStatus("ws send fail");
    if (audio_) audio_->Play(drivers::SoundId::Error);
    EnterIdle();
    return;
  }
  ws_.Tick();
  if (!audio_io_.MicStart()) {
    SetStatus("mic fail");
    if (audio_) audio_->Play(drivers::SoundId::Error);
    EnterIdle();
    return;
  }
  mic_run_ = true;
  spk_run_ = false;
  listen_started_ms_ = millis();
  state_ = State::Listen;
  if (audio_) audio_->SetExclusive(true);
  SetStatus("listening");
}

void VoiceAssistant::EnterThinking() {
  mic_run_ = false;
  audio_io_.MicStop();
  FlushTxQueue();
  ptt_session_ = false;
  ptt_held_ = false;
  ptt_release_ms_ = 0;
  audio_io_.EnsureSpeaker();
  if (audio_) audio_->SetExclusive(true);
  ws_.SendTxt("{\"event\":\"end\"}");
  state_ = State::Thinking;
  last_rx_ms_ = millis();
  SetStatus("thinking");
}

void VoiceAssistant::EnterSpeak() {
  audio_io_.MicStop();
  vTaskDelay(pdMS_TO_TICKS(40));
  // Force clean Speaker.begin after mic (ADV ES8311)
  audio_io_.StopPlayback();
  if (!audio_io_.EnsureSpeaker()) {
    SetStatus("spk fail");
    if (audio_) {
      audio_->SetExclusive(false);
      audio_->Play(drivers::SoundId::Error);
    }
    EnterIdle();
    return;
  }
  if (audio_) audio_->SetExclusive(true);
  if (!audio_io_.PlayTestBeep()) {
    SetStatus("beep fail");
  }
  spk_run_ = true;
  state_ = State::Speak;
  last_rx_ms_ = millis();
  SetStatus("speaking");
}

void VoiceAssistant::MicTaskEntry(void* arg) {
  static_cast<VoiceAssistant*>(arg)->MicTask();
}

void VoiceAssistant::SpkTaskEntry(void* arg) {
  static_cast<VoiceAssistant*>(arg)->SpkTask();
}

void VoiceAssistant::MicTask() {
  int16_t chunk[kChunkSamples];
  for (;;) {
    if (!began_ || !enabled_ || state_ != State::Listen || !mic_run_) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    if (!audio_io_.ReadChunk(chunk, kChunkSamples)) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    const VadEvent ev = vad_.Feed(chunk, kChunkSamples);
    if (tx_queue_) {
      if (xQueueSend(tx_queue_, chunk, 0) != pdTRUE) {
        int16_t dump[kChunkSamples];
        xQueueReceive(tx_queue_, dump, 0);
        xQueueSend(tx_queue_, chunk, 0);
      }
    }
    const bool maxed = (millis() - listen_started_ms_ >= kMaxListenMs);
    const bool vad_end = (ev == VadEvent::SpeechEnd) && !ptt_session_;
    if (vad_end || maxed) mic_run_ = false;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void VoiceAssistant::SpkTask() {
  int16_t pcm[kChunkSamples];
  uint32_t fail = 0;
  uint32_t ok_n = 0;
  for (;;) {
    if (!spk_run_ || state_ != State::Speak) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    const size_t n = ws_.PopRx(reinterpret_cast<uint8_t*>(pcm), kChunkBytes);
    if (n >= sizeof(int16_t)) {
      last_rx_ms_ = millis();
      const size_t samples = n / sizeof(int16_t);
      if (audio_io_.PlayChunk(pcm, samples)) {
        ++rx_chunks_;
        ++ok_n;
        fail = 0;
        if (ok_n == 1) SetStatus("pcm play");
      } else if (++fail == 1) {
        SetStatus("play fail");
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

void VoiceAssistant::Tick() {
  if (!began_) return;
  if (!enabled_ && state_ == State::Idle) return;

  if (state_ == State::Idle) {
    if (cancel_req_) cancel_req_ = false;
    if (force_listen_) {
      force_listen_ = false;
      EnterListen(false);
    }
    return;
  }

  ws_.Tick();

  if (!ws_.IsConnected() && (state_ == State::Thinking || state_ == State::Speak)) {
    SetStatus("ws lost");
    audio_io_.StopPlayback();
    EnterIdle();
    return;
  }

  if (cancel_req_) {
    ws_.SendTxt("{\"event\":\"cancel\"}");
    audio_io_.StopPlayback();
    EnterIdle();
    return;
  }

  if (state_ == State::Listen) {
    int16_t chunk[kChunkSamples];
    uint8_t budget = kListenTxBudget;
    while (budget-- && tx_queue_ && xQueueReceive(tx_queue_, chunk, 0) == pdTRUE) {
      if (ws_.SendBin(reinterpret_cast<const uint8_t*>(chunk), kChunkBytes)) {
        ++tx_chunks_;
      } else {
        break;
      }
    }
    if (ptt_session_ && !ptt_held_ && ptt_release_ms_ != 0) {
      if (millis() - ptt_release_ms_ >= kPttHangoverMs) {
        ptt_release_ms_ = 0;
        mic_run_ = false;
        SetStatus("ending...");
      }
    }
    if (!mic_run_) EnterThinking();
    return;
  }

  if (state_ == State::Thinking) {
    if (ws_.RxPending() > 0) {
      EnterSpeak();
      return;
    }
    if (millis() - last_rx_ms_ > kSpeakIdleTimeoutMs) {
      SetStatus("no reply");
      if (audio_) {
        audio_->SetExclusive(false);
        audio_->Play(drivers::SoundId::Error);
      }
      EnterIdle();
    }
    return;
  }

  if (state_ == State::Speak) {
    const bool quiet = !audio_io_.IsPlaying() && ws_.RxPending() == 0 &&
                       (millis() - last_rx_ms_ > kSpeakQuietExitMs);
    if (quiet || (millis() - last_rx_ms_ > kSpeakIdleTimeoutMs)) {
      SetStatus(quiet ? "speak done" : "speak timeout");
      spk_run_ = false;
      vTaskDelay(pdMS_TO_TICKS(200));
      EnterIdle();
    }
  }
}

}  // namespace axiom::voice
