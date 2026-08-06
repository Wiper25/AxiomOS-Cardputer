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

 private:
  void StartSequence(const Note* notes, uint8_t count);

  const Note* active_notes_ = nullptr;
  uint8_t active_count_ = 0;
  uint8_t active_index_ = 0;
  uint32_t note_end_at_ms_ = 0;
};

}  // namespace axiom::drivers
