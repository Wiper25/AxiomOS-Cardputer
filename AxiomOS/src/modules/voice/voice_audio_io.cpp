#include "modules/voice/voice_audio_io.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include <string.h>

namespace axiom::voice {
namespace {

void ApplySpeakerConfig() {
  auto spk_cfg = M5.Speaker.config();
  spk_cfg.sample_rate = kSampleRateHz;
  spk_cfg.stereo = false;
  spk_cfg.magnification = kSpkMagnification;
  spk_cfg.dma_buf_count = 8;
  spk_cfg.dma_buf_len = 256;
  spk_cfg.task_priority = 4;
  M5.Speaker.config(spk_cfg);
}

int16_t FramePeak(const int16_t* pcm, size_t samples) {
  int16_t peak = 0;
  for (size_t i = 0; i < samples; ++i) {
    int16_t a = pcm[i] < 0 ? static_cast<int16_t>(-pcm[i]) : pcm[i];
    if (a > peak) peak = a;
  }
  return peak;
}

// Soft knee — never slam NS4150 into rails
int16_t SoftLimit(int32_t v) {
  if (v > kSoftKnee) {
    const int32_t over = v - kSoftKnee;
    v = kSoftKnee + over / 5;
    if (v > kSoftCeil) v = kSoftCeil;
  } else if (v < -kSoftKnee) {
    const int32_t over = -kSoftKnee - v;
    v = -kSoftKnee - over / 5;
    if (v < -kSoftCeil) v = -kSoftCeil;
  }
  return static_cast<int16_t>(v);
}

}  // namespace

void VoiceAudioIo::ResetPlayEq() {
  eq_x1_ = 0;
  eq_hp_ = 0;
  eq_lp_ = 0;
}

void VoiceAudioIo::ProcessPlayChunk(const int16_t* pcm, size_t samples, int16_t* out) {
  // 1) Per-chunk peak compress toward target — flattens ElevenLabs bursts
  const int16_t peak = FramePeak(pcm, samples);
  int32_t scale_num = kPlayGainNum;
  int32_t scale_den = kPlayGainDen;
  if (peak > kChunkTargetPeak) {
    scale_num = static_cast<int32_t>(kChunkTargetPeak) * kPlayGainNum;
    scale_den = static_cast<int32_t>(peak) * kPlayGainDen;
    if (scale_den < 1) scale_den = 1;
  }

  for (size_t i = 0; i < samples; ++i) {
    int32_t x = (static_cast<int32_t>(pcm[i]) * scale_num) / scale_den;

    // 2) HPF ~250 Hz — kill rumble that rattles the tiny cone
    const int32_t dx = x - eq_x1_;
    eq_x1_ = x;
    eq_hp_ = (kHpAlphaNum * (eq_hp_ + dx)) / kHpAlphaDen;
    x = eq_hp_;

    // 3) Presence: boost mid/high relative to slow LP (speech intelligibility)
    eq_lp_ += (x - eq_lp_) >> kPresenceLpShift;
    x = x + ((x - eq_lp_) * kPresenceNum) / kPresenceDen;

    out[i] = SoftLimit(x);
  }
}

bool VoiceAudioIo::Begin() {
  if (began_) return true;
  auto mic_cfg = M5.Mic.config();
  mic_cfg.sample_rate = kSampleRateHz;
  mic_cfg.magnification = 16;
  M5.Mic.config(mic_cfg);
  ApplySpeakerConfig();
  began_ = true;
  mic_running_ = false;
  spk_ready_ = false;
  ResetPlayEq();
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
    vTaskDelay(pdMS_TO_TICKS(40));
  }
  if (!spk_ready_) {
    ApplySpeakerConfig();
    M5.Speaker.end();
    vTaskDelay(pdMS_TO_TICKS(20));
    if (!M5.Speaker.begin()) return false;
    M5.Speaker.setVolume(kSpkVolume);
    spk_ready_ = true;
    ResetPlayEq();
  } else {
    M5.Speaker.setVolume(kSpkVolume);
  }
  return true;
}

bool VoiceAudioIo::MicStart() {
  if (!began_ && !Begin()) return false;

  StopPlayback();
  if (spk_ready_ || M5.Speaker.isEnabled()) {
    M5.Speaker.stop();
    M5.Speaker.end();
    spk_ready_ = false;
  }
  if (mic_running_) {
    M5.Mic.end();
    mic_running_ = false;
  }
  vTaskDelay(pdMS_TO_TICKS(60));

  auto mic_cfg = M5.Mic.config();
  mic_cfg.sample_rate = kSampleRateHz;
  mic_cfg.magnification = 16;
  M5.Mic.config(mic_cfg);

  for (int attempt = 0; attempt < 3; ++attempt) {
    if (M5.Mic.begin()) {
      mic_running_ = true;
      int16_t warm[kChunkSamples];
      for (int w = 0; w < 4; ++w) {
        (void)ReadChunk(warm, kChunkSamples);
      }
      return true;
    }
    M5.Mic.end();
    vTaskDelay(pdMS_TO_TICKS(40));
  }
  return false;
}

void VoiceAudioIo::MicStop() {
  if (!mic_running_) return;
  M5.Mic.end();
  mic_running_ = false;
}

void VoiceAudioIo::ReleaseBus() {
  StopPlayback();
  if (mic_running_) {
    M5.Mic.end();
    mic_running_ = false;
  }
  if (spk_ready_ || M5.Speaker.isEnabled()) {
    M5.Speaker.stop();
    M5.Speaker.end();
    spk_ready_ = false;
  }
  vTaskDelay(pdMS_TO_TICKS(30));
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

  if (FramePeak(pcm, samples) < 64) {
    play_queued_ = 1;
    return true;
  }

  M5.Speaker.setVolume(kSpkVolume);
  int16_t* slot = play_buf_[play_toggle_ % kPlayBufCount];
  ++play_toggle_;
  ProcessPlayChunk(pcm, samples, slot);

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
  M5.Speaker.stop();
  M5.Speaker.setVolume(kSpkVolume);
  ResetPlayEq();

  int16_t* slot = play_buf_[0];
  const float w = 2.f * 3.1415926f * 880.f / static_cast<float>(kSampleRateHz);
  for (size_t i = 0; i < kChunkSamples; ++i) {
    slot[i] = SoftLimit(static_cast<int32_t>(7000.f * sinf(w * static_cast<float>(i))));
  }
  const bool ok =
      M5.Speaker.playRaw(slot, kChunkSamples, kSampleRateHz, false, 1, kTtsChannel, true);
  if (ok) {
    int16_t* slot2 = play_buf_[1];
    for (size_t i = 0; i < kChunkSamples; ++i) {
      slot2[i] = SoftLimit(
          static_cast<int32_t>(5500.f * sinf(w * static_cast<float>(i + kChunkSamples))));
    }
    M5.Speaker.playRaw(slot2, kChunkSamples, kSampleRateHz, false, 1, kTtsChannel, false);
    vTaskDelay(pdMS_TO_TICKS(45));
  }
  play_toggle_ = 2;
  return ok;
}

bool VoiceAudioIo::IsPlaying() const { return spk_ready_ && M5.Speaker.isPlaying(); }

void VoiceAudioIo::StopPlayback() {
  if (spk_ready_) M5.Speaker.stop();
  play_queued_ = 0;
  ResetPlayEq();
}

}  // namespace axiom::voice
