#pragma once

#include <stddef.h>
#include <stdint.h>

#include "modules/ai/ai_config.h"

namespace axiom::ai {

enum class ChatRole : uint8_t { System = 0, User = 1, Assistant = 2 };

struct ChatMessage {
  ChatRole role = ChatRole::User;
  char text[kMaxMessageChars] = {0};
  uint32_t ms = 0;
};

enum class ConversationState : uint8_t {
  Idle = 0,
  Thinking,
  Streaming,
  Typing,
  Error
};

class AIConversation {
 public:
  void Clear();
  bool Add(ChatRole role, const char* text);
  uint16_t Count() const { return count_; }
  const ChatMessage& At(uint16_t i) const { return messages_[i]; }

  void BeginAssistantStream();
  bool AppendAssistantChunk(const char* chunk);
  void FinalizeAssistant();
  void SetError(const char* err);

  ConversationState State() const { return state_; }
  void SetState(ConversationState s) { state_ = s; }

  const char* StreamingBuffer() const { return stream_buf_; }
  uint16_t StreamingLen() const { return stream_len_; }

  // Typing animation over last assistant message
  void StartTypingAnimation();
  void TickTyping(uint8_t chars_per_tick);
  uint16_t TypingVisibleChars() const { return typing_visible_; }
  bool TypingDone() const { return typing_done_; }

  bool BuildContextPrompt(char* dst, size_t n, const char* user_msg) const;

 private:
  void ShiftIfFull();

  ChatMessage messages_[kMaxChatMessages];
  uint16_t count_ = 0;
  ConversationState state_ = ConversationState::Idle;
  char stream_buf_[kMaxResponseChars] = {0};
  uint16_t stream_len_ = 0;
  uint16_t typing_visible_ = 0;
  bool typing_done_ = true;
};

}  // namespace axiom::ai
