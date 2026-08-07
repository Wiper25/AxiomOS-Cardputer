#include "modules/http/http_module.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <string.h>

namespace axiom::modules {

bool HttpModule::Begin() {
  telemetry_.state = HttpState::Idle;
  return true;
}

void HttpModule::Tick() {}

bool HttpModule::Request() {
  if (WiFi.status() != WL_CONNECTED) {
    telemetry_.state = HttpState::Error;
    telemetry_.code = 0;
    strncpy(telemetry_.preview, "no wifi", sizeof(telemetry_.preview) - 1);
    return false;
  }

  telemetry_.state = HttpState::Busy;
  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  const bool is_https = strncmp(config_.url, "https://", 8) == 0;
  WiFiClientSecure secure;
  bool began = false;
  if (is_https) {
    secure.setInsecure();
    began = http.begin(secure, config_.url);
  } else {
    began = http.begin(config_.url);
  }

  if (!began) {
    telemetry_.state = HttpState::Error;
    telemetry_.code = 0;
    strncpy(telemetry_.preview, "begin fail", sizeof(telemetry_.preview) - 1);
    return false;
  }

  int code = 0;
  if (config_.method == HttpMethod::Post) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(config_.body);
  } else {
    code = http.GET();
  }

  telemetry_.code = static_cast<int16_t>(code);
  String body = http.getString();
  http.end();

  const size_t n =
      body.length() < sizeof(telemetry_.preview) - 1 ? body.length() : sizeof(telemetry_.preview) - 1;
  memcpy(telemetry_.preview, body.c_str(), n);
  telemetry_.preview[n] = '\0';
  telemetry_.state = (code > 0 && code < 400) ? HttpState::Ok : HttpState::Error;
  return telemetry_.state == HttpState::Ok;
}

}  // namespace axiom::modules
