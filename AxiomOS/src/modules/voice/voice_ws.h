#pragma once

#include <WebSocketsClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <stddef.h>
#include <stdint.h>

#include "modules/voice/voice_config.h"

namespace axiom::voice {

enum class VoiceWsState : uint8_t { Idle = 0, Connecting, Connected, Error };

class VoiceWs {
 public:
  bool Begin();
  void Tick();
  bool Connect(const char* host, uint16_t port, const char* path);
  void Disconnect();
  bool IsConnected() const;
  VoiceWsState State() const { return state_; }

  bool SendBin(const uint8_t* data, size_t len);
  bool SendTxt(const char* text);

  // Pop one RX PCM chunk into dst (up to max_bytes). Returns bytes copied, 0 if empty.
  size_t PopRx(uint8_t* dst, size_t max_bytes);
  uint8_t RxPending() const { return rx_count_; }

  // Server TEXT {"event":"done"} — Speak can end without waiting quiet timeout
  bool ConsumeServerDone();

 private:
  static void OnEventThunk(WStype_t type, uint8_t* payload, size_t length);
  void OnEvent(WStype_t type, uint8_t* payload, size_t length);
  void PushRx(const uint8_t* data, size_t len);

  WebSocketsClient ws_;
  VoiceWsState state_ = VoiceWsState::Idle;
  static VoiceWs* instance_;

  struct RxSlot {
    uint16_t len = 0;
    uint8_t data[kChunkBytes];
  };
  RxSlot rx_[kRxQueueDepth];
  uint8_t rx_head_ = 0;
  uint8_t rx_tail_ = 0;
  uint8_t rx_count_ = 0;
  portMUX_TYPE rx_mux_ = portMUX_INITIALIZER_UNLOCKED;
  volatile bool server_done_ = false;
};

}  // namespace axiom::voice
