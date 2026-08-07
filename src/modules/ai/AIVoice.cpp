#include "modules/ai/AIVoice.h"

#include <string.h>

namespace axiom::ai {

void AIVoice::Push(VoiceEventType t, const char* text) {
  if (q_count_ >= 4) {
    q_head_ = static_cast<uint8_t>((q_head_ + 1) % 4);
    --q_count_;
  }
  VoiceEvent& e = queue_[q_tail_];
  e.type = t;
  e.text[0] = 0;
  if (text) {
    strncpy(e.text, text, sizeof(e.text) - 1);
    e.text[sizeof(e.text) - 1] = 0;
  }
  q_tail_ = static_cast<uint8_t>((q_tail_ + 1) % 4);
  ++q_count_;
}

bool AIVoice::PollEvent(VoiceEvent& out) {
  if (q_count_ == 0) return false;
  out = queue_[q_head_];
  q_head_ = static_cast<uint8_t>((q_head_ + 1) % 4);
  --q_count_;
  return true;
}

void AIVoice::InjectTranscript(const char* text) {
  Push(VoiceEventType::WakeDetected, "Axiom");
  Push(VoiceEventType::FinalTranscript, text);
}

void AIVoice::Tick() {
  if (!enabled_ || !IsReady()) return;
  // Future: capture_->Read → wake_->Feed → stt_->Feed
  // Architecture reserved; no blocking audio path yet.
}

}  // namespace axiom::ai
