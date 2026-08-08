#pragma once

#include <stddef.h>
#include <stdint.h>

#include "modules/voice/voice_config.h"

namespace axiom::voice {

class VoiceAudioIo {
 public:
  bool Begin();
  void End();

  bool MicStart();
  void MicStop();
  bool MicRunning() const { return mic_running_; }

  bool EnsureSpeaker();
  // Full Mic+Speaker teardown (call on Idle after TTS so next Listen works)
  void ReleaseBus();

  bool ReadChunk(int16_t* dst, size_t samples = kChunkSamples);
  bool PlayChunk(const int16_t* pcm, size_t samples);
  bool PlayTestBeep();
  bool IsPlaying() const;
  void StopPlayback();
  uint8_t PlaybackQueued() const { return play_queued_; }

 private:
  void ResetPlayEq();
  void ProcessPlayChunk(const int16_t* pcm, size_t samples, int16_t* out);

  bool mic_running_ = false;
  bool spk_ready_ = false;
  bool began_ = false;
  static constexpr uint8_t kPlayBufCount = 16;
  static constexpr int kTtsChannel = 0;
  int16_t play_buf_[kPlayBufCount][kChunkSamples];
  uint8_t play_toggle_ = 0;
  volatile uint8_t play_queued_ = 0;
  // Speech EQ state (continuous across TTS chunks)
  int32_t eq_x1_ = 0;
  int32_t eq_hp_ = 0;
  int32_t eq_lp_ = 0;
};

}  // namespace axiom::voice
