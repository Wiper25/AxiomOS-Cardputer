#include "modules/ai/AIConversation.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace axiom::ai {

void AIConversation::Clear() {
  count_ = 0;
  state_ = ConversationState::Idle;
  stream_buf_[0] = 0;
  stream_len_ = 0;
  typing_visible_ = 0;
  typing_done_ = true;
  memset(messages_, 0, sizeof(messages_));
}

void AIConversation::ShiftIfFull() {
  if (count_ < kMaxChatMessages) return;
  memmove(&messages_[0], &messages_[1], sizeof(ChatMessage) * (kMaxChatMessages - 1));
  --count_;
  memset(&messages_[count_], 0, sizeof(ChatMessage));
}

bool AIConversation::Add(ChatRole role, const char* text) {
  if (text == nullptr || text[0] == 0) return false;
  ShiftIfFull();
  ChatMessage& m = messages_[count_++];
  m.role = role;
  m.ms = millis();
  strncpy(m.text, text, sizeof(m.text) - 1);
  m.text[sizeof(m.text) - 1] = 0;
  return true;
}

void AIConversation::BeginAssistantStream() {
  stream_buf_[0] = 0;
  stream_len_ = 0;
  state_ = ConversationState::Streaming;
}

bool AIConversation::AppendAssistantChunk(const char* chunk) {
  if (chunk == nullptr || chunk[0] == 0) return false;
  const size_t add = strlen(chunk);
  if (stream_len_ + add >= sizeof(stream_buf_)) {
    const size_t room = sizeof(stream_buf_) - 1 - stream_len_;
    if (room == 0) return false;
    memcpy(stream_buf_ + stream_len_, chunk, room);
    stream_len_ = static_cast<uint16_t>(sizeof(stream_buf_) - 1);
    stream_buf_[stream_len_] = 0;
    return true;
  }
  memcpy(stream_buf_ + stream_len_, chunk, add);
  stream_len_ = static_cast<uint16_t>(stream_len_ + add);
  stream_buf_[stream_len_] = 0;
  return true;
}

void AIConversation::FinalizeAssistant() {
  if (stream_len_ > 0) {
    Add(ChatRole::Assistant, stream_buf_);
  }
  const uint16_t len = stream_len_;
  stream_buf_[0] = 0;
  stream_len_ = 0;
  // Long replies: show fully immediately (scrollable). Short: typewriter.
  if (len > 48) {
    typing_visible_ = 0xFFFF;
    typing_done_ = true;
    state_ = ConversationState::Idle;
  } else {
    StartTypingAnimation();
  }
}

void AIConversation::SetError(const char* err) {
  state_ = ConversationState::Error;
  if (err && err[0]) {
    char buf[96];
    snprintf(buf, sizeof(buf), "[err] %s", err);
    Add(ChatRole::Assistant, buf);
  }
}

void AIConversation::StartTypingAnimation() {
  typing_visible_ = 0;
  typing_done_ = false;
  state_ = ConversationState::Typing;
}

void AIConversation::TickTyping(uint8_t chars_per_tick) {
  if (typing_done_ || count_ == 0) return;
  const ChatMessage& last = messages_[count_ - 1];
  if (last.role != ChatRole::Assistant) {
    typing_done_ = true;
    state_ = ConversationState::Idle;
    return;
  }
  const uint16_t len = static_cast<uint16_t>(strlen(last.text));
  typing_visible_ =
      static_cast<uint16_t>(typing_visible_ + chars_per_tick);
  if (typing_visible_ >= len) {
    typing_visible_ = len;
    typing_done_ = true;
    state_ = ConversationState::Idle;
  }
}

bool AIConversation::BuildContextPrompt(char* dst, size_t n, const char* user_msg) const {
  if (dst == nullptr || n < 16) return false;
  size_t off = 0;
  int w = snprintf(dst + off, n - off, "System: AxiomOS Cardputer assistant.\n");
  if (w > 0) off += static_cast<size_t>(w);

  const uint16_t start = (count_ > 4) ? static_cast<uint16_t>(count_ - 4) : 0;
  for (uint16_t i = start; i < count_; ++i) {
    const char* role = messages_[i].role == ChatRole::User       ? "User"
                       : messages_[i].role == ChatRole::Assistant ? "Assistant"
                                                                  : "System";
    w = snprintf(dst + off, n - off, "%s: %s\n", role, messages_[i].text);
    if (w < 0 || static_cast<size_t>(w) >= n - off) break;
    off += static_cast<size_t>(w);
  }
  snprintf(dst + off, n - off, "User: %s\nAssistant:", user_msg ? user_msg : "");
  return true;
}

}  // namespace axiom::ai
