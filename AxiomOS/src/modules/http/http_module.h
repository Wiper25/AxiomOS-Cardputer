#pragma once

#include <stdint.h>

namespace axiom::modules {

enum class HttpMethod : uint8_t { Get = 0, Post = 1 };

enum class HttpState : uint8_t { Idle = 0, Busy, Ok, Error };

struct HttpConfig {
  char url[96] = "http://httpbin.org/get";
  HttpMethod method = HttpMethod::Get;
  char body[48] = "{\"ok\":1}";
};

struct HttpTelemetry {
  HttpState state = HttpState::Idle;
  int16_t code = 0;
  char preview[64] = {0};
};

class HttpModule {
 public:
  bool Begin();
  void Tick();
  bool Request();
  HttpConfig& Config() { return config_; }
  HttpTelemetry GetTelemetry() const { return telemetry_; }

 private:
  HttpConfig config_;
  HttpTelemetry telemetry_;
};

}  // namespace axiom::modules
