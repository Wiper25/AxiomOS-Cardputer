#include "drivers/audio/audio_driver.h"

#include <Arduino.h>
#include <M5Cardputer.h>

namespace axiom::drivers {

namespace {
constexpr AudioDriver::Note kKeyClickSeq[] = {{1800, 22}};
constexpr AudioDriver::Note kMenuOpenSeq[] = {{900, 26}, {1350, 34}};
constexpr AudioDriver::Note kErrorSeq[] = {{420, 80}, {280, 130}};
constexpr AudioDriver::Note kSuccessSeq[] = {{1100, 45}, {1450, 60}};
constexpr AudioDriver::Note kBootSeq[] = {{620, 60}, {880, 60}, {1240, 90}, {1480, 120}};
}  // namespace

bool AudioDriver::Begin() {
  M5.Speaker.setVolume(96);
  return true;
}

void AudioDriver::Tick() {
  // Never touch Speaker while mic owns ES8311 — symptom: uplink level=8 after frame#1
  if (exclusive_) {
    if (active_notes_ != nullptr) Stop();
    return;
  }
  if (active_notes_ == nullptr || active_index_ >= active_count_) {
    return;
  }
  if (millis() < note_end_at_ms_) {
    return;
  }

  ++active_index_;
  if (active_index_ >= active_count_) {
    active_notes_ = nullptr;
    active_count_ = 0;
    return;
  }

  const Note& n = active_notes_[active_index_];
  M5.Speaker.tone(n.freq, n.dur_ms);
  note_end_at_ms_ = millis() + n.dur_ms;
}

void AudioDriver::Stop() {
  active_notes_ = nullptr;
  active_count_ = 0;
  active_index_ = 0;
  note_end_at_ms_ = 0;
  M5.Speaker.stop();
}

void AudioDriver::Play(SoundId sound) {
  if (exclusive_) return;
  switch (sound) {
    case SoundId::KeyClick:
      StartSequence(kKeyClickSeq, sizeof(kKeyClickSeq) / sizeof(kKeyClickSeq[0]));
      break;
    case SoundId::MenuOpen:
      StartSequence(kMenuOpenSeq, sizeof(kMenuOpenSeq) / sizeof(kMenuOpenSeq[0]));
      break;
    case SoundId::Error:
      StartSequence(kErrorSeq, sizeof(kErrorSeq) / sizeof(kErrorSeq[0]));
      break;
    case SoundId::Success:
      StartSequence(kSuccessSeq, sizeof(kSuccessSeq) / sizeof(kSuccessSeq[0]));
      break;
    case SoundId::BootMelody:
      StartSequence(kBootSeq, sizeof(kBootSeq) / sizeof(kBootSeq[0]));
      break;
    default:
      break;
  }
}

void AudioDriver::SetVolume(uint8_t v) { M5.Speaker.setVolume(v); }

void AudioDriver::StartSequence(const Note* notes, uint8_t count) {
  if (notes == nullptr || count == 0) {
    return;
  }
  active_notes_ = notes;
  active_count_ = count;
  active_index_ = 0;
  const Note& n = active_notes_[0];
  M5.Speaker.tone(n.freq, n.dur_ms);
  note_end_at_ms_ = millis() + n.dur_ms;
}

}  // namespace axiom::drivers
