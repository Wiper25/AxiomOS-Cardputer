#include "modules/voice/voice_audio_io.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include <string.h>

namespace axiom::voice {

bool VoiceAudioIo::Begin() {
  if (began_) return true;
  auto mic_cfg = M5.Mic.config();
  mic_cfg.sample_rate = kSampleRateHz;
  mic_cfg.magnification = 16;
  M5.Mic.config(mic_cfg);

  auto spk_cfg = M5.Speaker.config();
  spk_cfg.sample_rate = kSampleRateHz;
  spk_cfg.stereo = false;
  if (spk_cfg.magnification < 64) spk_cfg.magnification = 64;
  spk_cfg.dma_buf_count = 8;
  spk_cfg.dma_buf_len = 256;
  M5.Speaker.config(spk_cfg);

  began_ = true;
  mic_running_ = false;
  spk_ready_ = false;
  return true;
}

void VoiceAudioIo::End() {
  MicStop();
  StopPlayback();
  if (spk_ready_) {
    M5.Speaker.end();
    spk_ready_ = false;
  }
  began_ = false;
}

bool VoiceAudioIo::EnsureSpeaker() {
  if (!began_ && !Begin()) return false;
  if (mic_running_) {
    M5.Mic.end();
    mic_running_ = false;
  }
  if (!spk_ready_) {
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.sample_rate = kSampleRateHz;
    spk_cfg.stereo = false;
    if (spk_cfg.magnification < 64) spk_cfg.magnification = 64;
    spk_cfg.dma_buf_count = 8;
    spk_cfg.dma_buf_len = 256;
    M5.Speaker.config(spk_cfg);
    if (!M5.Speaker.begin()) return false;
    M5.Speaker.setVolume(255);
    spk_ready_ = true;
  } else {
    M5.Speaker.setVolume(255);
  }
  return true;
}

bool VoiceAudioIo::MicStart() {
  if (!began_ && !Begin()) return false;
  if (mic_running_) return true;
  StopPlayback();
  if (spk_ready_) {
    M5.Speaker.end();
    spk_ready_ = false;
  }
  if (!M5.Mic.begin()) return false;
  mic_running_ = true;
  return true;
}

void VoiceAudioIo::MicStop() {
  if (!mic_running_) return;
  M5.Mic.end();
  mic_running_ = false;
}

bool VoiceAudioIo::ReadChunk(int16_t* dst, size_t samples) {
  if (!mic_running_ || dst == nullptr || samples == 0) return false;
  const uint32_t deadline = millis() + 40;
  while (!M5.Mic.record(dst, samples, kSampleRateHz)) {
    if (millis() > deadline) return false;
    delay(1);
  }
  return true;
}

bool VoiceAudioIo::PlayChunk(const int16_t* pcm, size_t samples) {
  if (pcm == nullptr || samples == 0 || samples > kChunkSamples) return false;
  if (!EnsureSpeaker()) return false;
  M5.Speaker.setVolume(255);

  int16_t* slot = play_buf_[play_toggle_ % kPlayBufCount];
  ++play_toggle_;
  for (size_t i = 0; i < samples; ++i) {
    int32_t v = static_cast<int32_t>(pcm[i]) * 2;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    slot[i] = static_cast<int16_t>(v);
  }

  for (int attempt = 0; attempt < 200; ++attempt) {
    if (M5.Speaker.playRaw(slot, samples, kSampleRateHz, false, 1, kTtsChannel, false)) {
      play_queued_ = 1;
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  return false;
}

bool VoiceAudioIo::PlayTestBeep() {
  if (!EnsureSpeaker()) return false;
  M5.Speaker.setVolume(255);
  int16_t* slot = play_buf_[0];
  const float w = 2.f * 3.1415926f * 880.f / static_cast<float>(kSampleRateHz);
  for (size_t i = 0; i < kChunkSamples; ++i) {
    slot[i] = static_cast<int16_t>(20000.f * sinf(w * static_cast<float>(i)));
  }
  const bool ok =
      M5.Speaker.playRaw(slot, kChunkSamples, kSampleRateHz, false, 1, kTtsChannel, true);
  play_toggle_ = 1;
  return ok;
}

bool VoiceAudioIo::IsPlaying() const { return spk_ready_ && M5.Speaker.isPlaying(); }

void VoiceAudioIo::StopPlayback() {
  if (spk_ready_) M5.Speaker.stop();
  play_queued_ = 0;
}

}  // namespace axiom::voice
