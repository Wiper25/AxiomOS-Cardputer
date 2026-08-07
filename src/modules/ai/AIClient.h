#pragma once

#include <stddef.h>
#include <stdint.h>

#include "modules/ai/ai_config.h"
#include "modules/ai/AISettings.h"

namespace axiom::ai {

class AIConversation;

enum class ClientState : uint8_t {
  Idle = 0,
  Connecting,
  Sending,
  Streaming,
  Done,
  Error
};

class AIClient {
 public:
  void Configure(const AiSettingsData& settings);
  void Tick();

  bool RequestChat(const char* prompt, bool stream);
  // OpenAI-style: system + recent turns (excludes duplicate trailing user)
  bool RequestChatMessages(const char* system, const AIConversation& conv,
                           const char* user_text, bool stream);
  void Abort();

  ClientState State() const { return state_; }
  const char* LastError() const { return error_; }
  bool TakeChunk(char* dst, size_t n);
  void OnWsPayload(const char* text);
  bool IsBusy() const {
    return state_ == ClientState::Connecting || state_ == ClientState::Sending ||
           state_ == ClientState::Streaming;
  }

 private:
  bool DoRest(const char* prompt, bool stream);
  bool DoRestMessages(const char* system, const AIConversation& conv, const char* user_text,
                      bool stream);
  bool DoWebSocket(const char* prompt, bool stream);
  void ParseSseOrJsonChunk(const char* data, size_t len);
  void ExtractDeltaContent(const char* json_line);
  static bool ExtractJsonContentString(const char* json, char* dst, size_t dst_n);
  void SetError(const char* msg);
  void PushChunk(const char* s);
  void PushTextAsChunks(const char* text);

  AiSettingsData cfg_;
  ClientState state_ = ClientState::Idle;
  char error_[64] = {0};
  static constexpr uint8_t kChunkQSize = 12;
  char chunk_q_[kChunkQSize][kMaxStreamChunk];
  uint8_t chunk_head_ = 0;
  uint8_t chunk_tail_ = 0;
  uint8_t chunk_count_ = 0;
  bool ws_active_ = false;
  uint32_t request_started_ms_ = 0;
};

}  // namespace axiom::ai
