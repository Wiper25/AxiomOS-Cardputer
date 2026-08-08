#pragma once

#include <stdint.h>

namespace axiom::drivers {

enum class SoundId : uint8_t {
  KeyClick = 0,
  MenuOpen,
  Error,
  Success,
  BootMelody
};

class AudioDriver {
 public:
  struct Note {
    uint16_t freq;
    uint16_t dur_ms;
  };

  bool Begin();
  void Tick();
  void Play(SoundId sound);
  void SetVolume(uint8_t v);
  // Abort multi-note sequence — MUST call before MicStart (Tick→tone kills ADC).
  void Stop();
  // When true, UI tones are suppressed (voice owns speaker).
  void SetExclusive(bool v) { exclusive_ = v; }
  bool IsExclusive() const { return exclusive_; }

 private:
  void StartSequence(const Note* notes, uint8_t count);

  const Note* active_notes_ = nullptr;
  uint8_t active_count_ = 0;
  uint8_t active_index_ = 0;
  uint32_t note_end_at_ms_ = 0;
  bool exclusive_ = false;
};

}  // namespace axiom::drivers
