#pragma once

#include <stdint.h>

namespace axiom::modules {

enum class PingState : uint8_t { Idle = 0, Busy, Ok, Error };

struct PingConfig {
  char host[48] = "8.8.8.8";
};

struct PingTelemetry {
  PingState state = PingState::Idle;
  bool resolved = false;
  char ip[16] = {0};
  float avg_ms = 0;
  uint8_t loss_percent = 100;
};

class PingModule {
 public:
  bool Begin();
  void Tick();
  bool Resolve();
  bool Run();
  PingConfig& Config() { return config_; }
  PingTelemetry GetTelemetry() const { return telemetry_; }

 private:
  PingConfig config_;
  PingTelemetry telemetry_;
};

}  // namespace axiom::modules
