#include "modules/voice/voice_vad.h"

#include <Arduino.h>
#include <stdlib.h>

namespace axiom::voice {

void EnergyVad::Reset() {
  in_speech_ = false;
  loud_run_ = 0;
  quiet_run_ = 0;
  last_level_ = 0;
  speech_started_ms_ = 0;
}

uint32_t EnergyVad::MeanAbs(const int16_t* pcm, size_t samples) const {
  if (!pcm || samples == 0) return 0;
  uint64_t sum = 0;
  for (size_t i = 0; i < samples; ++i) {
    sum += static_cast<uint32_t>(abs(static_cast<int>(pcm[i])));
  }
  return static_cast<uint32_t>(sum / samples);
}

VadEvent EnergyVad::Feed(const int16_t* pcm, size_t samples) {
  last_level_ = MeanAbs(pcm, samples);

  if (!in_speech_) {
    if (last_level_ >= kVadStartThreshold) {
      if (++loud_run_ >= kVadStartFrames) {
        in_speech_ = true;
        quiet_run_ = 0;
        speech_started_ms_ = millis();
        return VadEvent::SpeechStart;
      }
    } else {
      loud_run_ = 0;
    }
    return VadEvent::None;
  }

  // in speech
  if (last_level_ < kVadEndThreshold) {
    ++quiet_run_;
  } else {
    quiet_run_ = 0;
  }

  const uint32_t elapsed = millis() - speech_started_ms_;
  const bool silence_done =
      (quiet_run_ * (kChunkSamples * 1000U / kSampleRateHz)) >= kSilenceEndMs;
  const bool timed_out = elapsed >= kMaxListenMs;

  if (silence_done || timed_out) {
    in_speech_ = false;
    loud_run_ = 0;
    quiet_run_ = 0;
    return VadEvent::SpeechEnd;
  }
  return VadEvent::None;
}

}  // namespace axiom::voice
