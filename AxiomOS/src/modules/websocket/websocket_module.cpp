#include "modules/websocket/websocket_module.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

namespace axiom::modules {

WebsocketModule* WebsocketModule::instance_ = nullptr;

bool WebsocketModule::Begin() {
  instance_ = this;
  telemetry_.state = WsState::Idle;
  return true;
}

void WebsocketModule::OnEventThunk(WStype_t type, uint8_t* payload, size_t length) {
  if (instance_ != nullptr) instance_->OnEvent(type, payload, length);
}

void WebsocketModule::OnEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      telemetry_.state = WsState::Connected;
      break;
    case WStype_DISCONNECTED:
      if (telemetry_.state == WsState::Connected || telemetry_.state == WsState::Connecting) {
        telemetry_.state = WsState::Idle;
      }
      break;
    case WStype_TEXT:
    case WStype_BIN: {
      const size_t n = length < sizeof(telemetry_.last_rx) - 1 ? length : sizeof(telemetry_.last_rx) - 1;
      if (payload != nullptr && n > 0) {
        memcpy(telemetry_.last_rx, payload, n);
      }
      telemetry_.last_rx[n] = '\0';
      telemetry_.has_rx = true;
      break;
    }
    case WStype_ERROR:
      telemetry_.state = WsState::Error;
      break;
    default:
      break;
  }
}

bool WebsocketModule::Connect() {
  if (WiFi.status() != WL_CONNECTED) {
    telemetry_.state = WsState::Error;
    return false;
  }
  Disconnect();
  telemetry_.state = WsState::Connecting;
  ws_.begin(config_.host, config_.port, config_.path);
  ws_.onEvent(OnEventThunk);
  ws_.setReconnectInterval(0);
  return true;
}

void WebsocketModule::Disconnect() {
  ws_.disconnect();
  telemetry_.state = WsState::Idle;
}

bool WebsocketModule::Send() {
  if (!ws_.isConnected()) {
    if (!Connect()) return false;
  }
  return ws_.sendTXT(config_.message);
}

void WebsocketModule::Tick() {
  ws_.loop();
  if (ws_.isConnected()) {
    telemetry_.state = WsState::Connected;
  } else if (telemetry_.state == WsState::Connected) {
    telemetry_.state = WsState::Idle;
  }
}

}  // namespace axiom::modules
