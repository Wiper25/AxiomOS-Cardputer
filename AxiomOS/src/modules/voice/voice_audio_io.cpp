#include "modules/voice/voice_audio_io.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <string.h>

namespace axiom::voice {

bool VoiceAudioIo::Begin() {
  if (began_) return true;
  auto mic_cfg = M5.Mic.config();
  mic_cfg.sample_rate = kSampleRateHz;
  // Cardputer ADV MEMS is quiet by default — boost for ASR
  mic_cfg.magnification = 16;
  M5.Mic.config(mic_cfg);
  // Do NOT start Mic here — keeps I2S free for UI speaker until Listen.
  began_ = true;
  mic_running_ = false;
  return true;
}

void VoiceAudioIo::End() {
  MicStop();
  StopPlayback();
  began_ = false;
}

bool VoiceAudioIo::MicStart() {
  if (!began_) {
    if (!Begin()) return false;
  }
  if (mic_running_) return true;
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
  uint32_t spins = 0;
  while (M5.Speaker.isPlaying() && spins++ < 200) {
    delay(1);
  }
  int16_t* slot = (play_toggle_++ & 1) ? play_b_ : play_a_;
  memcpy(slot, pcm, samples * sizeof(int16_t));
  const bool ok = M5.Speaker.playRaw(slot, samples, kSampleRateHz, false, 1, 0);
  if (ok) play_queued_ = 1;
  return ok;
}

bool VoiceAudioIo::IsPlaying() const { return M5.Speaker.isPlaying(); }

void VoiceAudioIo::StopPlayback() {
  M5.Speaker.stop();
  play_queued_ = 0;
}

}  // namespace axiom::voice
