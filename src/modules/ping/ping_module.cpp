#include "modules/ping/ping_module.h"

#include <Arduino.h>
#include <ESP32Ping.h>
#include <WiFi.h>
#include <string.h>

namespace axiom::modules {

bool PingModule::Begin() {
  telemetry_.state = PingState::Idle;
  return true;
}

void PingModule::Tick() {}

bool PingModule::Resolve() {
  if (WiFi.status() != WL_CONNECTED) {
    telemetry_.state = PingState::Error;
    telemetry_.resolved = false;
    return false;
  }
  IPAddress ip;
  if (!WiFi.hostByName(config_.host, ip)) {
    telemetry_.resolved = false;
    telemetry_.ip[0] = '\0';
    telemetry_.state = PingState::Error;
    return false;
  }
  snprintf(telemetry_.ip, sizeof(telemetry_.ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  telemetry_.resolved = true;
  return true;
}

bool PingModule::Run() {
  if (WiFi.status() != WL_CONNECTED) {
    telemetry_.state = PingState::Error;
    return false;
  }
  telemetry_.state = PingState::Busy;
  Resolve();

  const bool ok = Ping.ping(config_.host, 3);
  if (!ok) {
    telemetry_.state = PingState::Error;
    telemetry_.avg_ms = 0;
    telemetry_.loss_percent = 100;
    return false;
  }
  telemetry_.avg_ms = Ping.averageTime();
  // ESP32Ping doesn't expose loss cleanly; approximate
  telemetry_.loss_percent = 0;
  telemetry_.state = PingState::Ok;
  return true;
}

}  // namespace axiom::modules
