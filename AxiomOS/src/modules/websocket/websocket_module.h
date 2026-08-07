#pragma once

#include <WebSocketsClient.h>
#include <stdint.h>

namespace axiom::modules {

enum class WsState : uint8_t { Idle = 0, Connecting, Connected, Error };

struct WsConfig {
  char host[48] = "echo.websocket.events";
  uint16_t port = 80;
  char path[40] = "/";
  char message[48] = "hello";
};

struct WsTelemetry {
  WsState state = WsState::Idle;
  bool has_rx = false;
  char last_rx[64] = {0};
};

class WebsocketModule {
 public:
  bool Begin();
  void Tick();
  bool Connect();
  void Disconnect();
  bool Send();
  bool IsConnected() const { return telemetry_.state == WsState::Connected; }
  WsConfig& Config() { return config_; }
  WsTelemetry GetTelemetry() const { return telemetry_; }

 private:
  static void OnEventThunk(WStype_t type, uint8_t* payload, size_t length);
  void OnEvent(WStype_t type, uint8_t* payload, size_t length);

  WebSocketsClient ws_;
  WsConfig config_;
  WsTelemetry telemetry_;
  static WebsocketModule* instance_;
};

}  // namespace axiom::modules
