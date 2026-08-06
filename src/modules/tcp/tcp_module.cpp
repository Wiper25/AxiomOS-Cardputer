#include "modules/tcp/tcp_module.h"

#include <Arduino.h>
#include <string.h>

namespace axiom::modules {

bool TcpModule::Begin() {
  telemetry_.state = TcpState::Idle;
  return true;
}

void TcpModule::PollRx() {
  if (!client_.connected() || !client_.available()) return;
  size_t n = 0;
  while (client_.available() && n < sizeof(telemetry_.last_rx) - 1) {
    telemetry_.last_rx[n++] = static_cast<char>(client_.read());
  }
  telemetry_.last_rx[n] = '\0';
  telemetry_.has_rx = n > 0;
}

bool TcpModule::Connect() {
  if (WiFi.status() != WL_CONNECTED) {
    telemetry_.state = TcpState::Error;
    return false;
  }
  Disconnect();
  telemetry_.state = TcpState::Connecting;
  client_.setTimeout(5000);
  if (!client_.connect(config_.host, config_.port)) {
    telemetry_.state = TcpState::Error;
    return false;
  }
  telemetry_.state = TcpState::Connected;
  return true;
}

void TcpModule::Disconnect() {
  if (client_.connected()) client_.stop();
  telemetry_.state = TcpState::Idle;
}

bool TcpModule::Send() {
  if (!client_.connected()) {
    if (!Connect()) return false;
  }
  const size_t n = strlen(config_.message);
  const size_t wrote = client_.write(reinterpret_cast<const uint8_t*>(config_.message), n);
  PollRx();
  return wrote == n;
}

void TcpModule::Tick() {
  if (client_.connected()) {
    telemetry_.state = TcpState::Connected;
    PollRx();
  } else if (telemetry_.state == TcpState::Connected) {
    telemetry_.state = TcpState::Idle;
  }
}

}  // namespace axiom::modules
