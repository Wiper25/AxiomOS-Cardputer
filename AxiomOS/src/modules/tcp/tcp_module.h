#pragma once

#include <WiFi.h>
#include <stdint.h>

namespace axiom::modules {

enum class TcpState : uint8_t { Idle = 0, Connecting, Connected, Error };

struct TcpConfig {
  char host[48] = "tcpbin.com";
  uint16_t port = 4242;
  char message[48] = "hello\n";
};

struct TcpTelemetry {
  TcpState state = TcpState::Idle;
  bool has_rx = false;
  char last_rx[64] = {0};
};

class TcpModule {
 public:
  bool Begin();
  void Tick();
  bool Connect();
  void Disconnect();
  bool Send();
  bool IsConnected() const { return telemetry_.state == TcpState::Connected; }
  TcpConfig& Config() { return config_; }
  TcpTelemetry GetTelemetry() const { return telemetry_; }

 private:
  void PollRx();

  WiFiClient client_;
  TcpConfig config_;
  TcpTelemetry telemetry_;
};

}  // namespace axiom::modules
