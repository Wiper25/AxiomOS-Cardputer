#include "modules/voice/voice_ws.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

namespace axiom::voice {

VoiceWs* VoiceWs::instance_ = nullptr;

bool VoiceWs::Begin() {
  instance_ = this;
  state_ = VoiceWsState::Idle;
  rx_head_ = rx_tail_ = rx_count_ = 0;
  return true;
}

void VoiceWs::OnEventThunk(WStype_t type, uint8_t* payload, size_t length) {
  if (instance_) instance_->OnEvent(type, payload, length);
}

void VoiceWs::PushRx(const uint8_t* data, size_t len) {
  if (!data || len == 0) return;
  portENTER_CRITICAL(&rx_mux_);
  if (rx_count_ >= kRxQueueDepth) {
    // drop oldest
    rx_head_ = static_cast<uint8_t>((rx_head_ + 1) % kRxQueueDepth);
    --rx_count_;
  }
  RxSlot& s = rx_[rx_tail_];
  const size_t n = len > kChunkBytes ? kChunkBytes : len;
  memcpy(s.data, data, n);
  s.len = static_cast<uint16_t>(n);
  rx_tail_ = static_cast<uint8_t>((rx_tail_ + 1) % kRxQueueDepth);
  ++rx_count_;
  portEXIT_CRITICAL(&rx_mux_);
}

void VoiceWs::OnEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      state_ = VoiceWsState::Connected;
      break;
    case WStype_DISCONNECTED:
      if (state_ == VoiceWsState::Connected || state_ == VoiceWsState::Connecting) {
        state_ = VoiceWsState::Idle;
      }
      break;
    case WStype_BIN:
      PushRx(payload, length);
      break;
    case WStype_TEXT:
      // control ACKs ignored for now
      break;
    case WStype_ERROR:
      state_ = VoiceWsState::Error;
      break;
    default:
      break;
  }
}

bool VoiceWs::Connect(const char* host, uint16_t port, const char* path) {
  if (WiFi.status() != WL_CONNECTED) {
    state_ = VoiceWsState::Error;
    return false;
  }
  Disconnect();
  state_ = VoiceWsState::Connecting;
  ws_.begin(host, port, path ? path : kDefaultWsPath);
  ws_.onEvent(OnEventThunk);
  ws_.setReconnectInterval(0);
  return true;
}

void VoiceWs::Disconnect() {
  ws_.disconnect();
  state_ = VoiceWsState::Idle;
  portENTER_CRITICAL(&rx_mux_);
  rx_head_ = rx_tail_ = rx_count_ = 0;
  portEXIT_CRITICAL(&rx_mux_);
}

bool VoiceWs::IsConnected() const { return state_ == VoiceWsState::Connected; }

bool VoiceWs::SendBin(const uint8_t* data, size_t len) {
  if (!ws_.isConnected() || !data || len == 0) return false;
  return ws_.sendBIN(data, len);
}

bool VoiceWs::SendTxt(const char* text) {
  if (!ws_.isConnected() || !text) return false;
  return ws_.sendTXT(text);
}

size_t VoiceWs::PopRx(uint8_t* dst, size_t max_bytes) {
  if (!dst || max_bytes == 0) return 0;
  portENTER_CRITICAL(&rx_mux_);
  if (rx_count_ == 0) {
    portEXIT_CRITICAL(&rx_mux_);
    return 0;
  }
  RxSlot& s = rx_[rx_head_];
  const size_t n = s.len < max_bytes ? s.len : max_bytes;
  memcpy(dst, s.data, n);
  rx_head_ = static_cast<uint8_t>((rx_head_ + 1) % kRxQueueDepth);
  --rx_count_;
  portEXIT_CRITICAL(&rx_mux_);
  return n;
}

void VoiceWs::Tick() {
  ws_.loop();
  if (ws_.isConnected()) {
    state_ = VoiceWsState::Connected;
  } else if (state_ == VoiceWsState::Connected) {
    state_ = VoiceWsState::Idle;
  }
}

}  // namespace axiom::voice
