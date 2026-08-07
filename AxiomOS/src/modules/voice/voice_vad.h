#pragma once

#include <stddef.h>
#include <stdint.h>

#include "modules/voice/voice_config.h"

namespace axiom::voice {

enum class VadEvent : uint8_t { None = 0, SpeechStart, SpeechEnd };

class EnergyVad {
 public:
  void Reset();
  VadEvent Feed(const int16_t* pcm, size_t samples);
  uint32_t LastLevel() const { return last_level_; }
  bool InSpeech() const { return in_speech_; }

 private:
  uint32_t MeanAbs(const int16_t* pcm, size_t samples) const;

  bool in_speech_ = false;
  uint8_t loud_run_ = 0;
  uint8_t quiet_run_ = 0;
  uint32_t last_level_ = 0;
  uint32_t speech_started_ms_ = 0;
};

}  // namespace axiom::voice
