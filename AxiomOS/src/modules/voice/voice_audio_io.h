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

  // Blocking-ish record into dst (kChunkSamples). Returns false if mic idle/fail.
  bool ReadChunk(int16_t* dst, size_t samples = kChunkSamples);

  // Queue PCM for DMA playback (copies into internal slot). Returns false if full.
  bool PlayChunk(const int16_t* pcm, size_t samples);
  bool IsPlaying() const;
  void StopPlayback();
  uint8_t PlaybackQueued() const { return play_queued_; }

 private:
  bool mic_running_ = false;
  bool began_ = false;
  // Double-buffer for playRaw lifetime
  int16_t play_a_[kChunkSamples];
  int16_t play_b_[kChunkSamples];
  uint8_t play_toggle_ = 0;
  volatile uint8_t play_queued_ = 0;
};

}  // namespace axiom::voice
