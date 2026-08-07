#pragma once

#include <stdint.h>

#include "modules/ai/ai_config.h"

namespace axiom::ai {

enum class AiTransport : uint8_t { Rest = 0, WebSocket = 1 };

struct AiSettingsData {
  bool enabled = true;
  bool local_only = false;
  bool stream = true;
  bool confirm_actions = true;
  bool voice_ready = false;
  AiTransport transport = AiTransport::Rest;
  char rest_host[48] = {0};
  uint16_t rest_port = kDefaultRestPort;
  char rest_path[48] = {0};
  char ws_host[48] = {0};
  uint16_t ws_port = kDefaultWsPort;
  char ws_path[40] = {0};
  char model[40] = {0};
  char api_key[80] = {0};
  bool use_https = kDefaultUseHttps;
  uint8_t temperature_x10 = 7;  // 0.7
};

class AISettings {
 public:
  bool Begin();
  bool Load();
  bool Save() const;

  AiSettingsData& Data() { return data_; }
  const AiSettingsData& Data() const { return data_; }

  void SetEnabled(bool v);
  void SetLocalOnly(bool v);
  void SetTransport(AiTransport t);
  void CycleTransport();
  void SetStream(bool v);
  void SetConfirmActions(bool v);

 private:
  void ApplyApiFromSource();

  AiSettingsData data_;
  bool dirty_ = false;
};

}  // namespace axiom::ai
