#pragma once

#include <stdint.h>

#include "modules/ai/ai_config.h"

namespace axiom {
namespace modules {
class WifiModule;
class Nrf24Module;
class MqttModule;
class SensorsModule;
}  // namespace modules
}  // namespace axiom

namespace axiom::ai {

enum class ActionId : uint8_t {
  None = 0,
  WifiScan,
  ScanNrf24,
  OpenSettings,
  MqttPublish,
  ShowLogs,
  RfSetChannel,
  DoctorRun,
  Count
};

enum class ActionRisk : uint8_t { Safe = 0, Confirm = 1, Dangerous = 2 };

struct ActionRequest {
  ActionId id = ActionId::None;
  ActionRisk risk = ActionRisk::Safe;
  char args[kMaxActionArgs] = {0};
  bool pending_confirm = false;
};

struct ActionContext {
  modules::WifiModule* wifi = nullptr;
  modules::Nrf24Module* nrf = nullptr;
  modules::MqttModule* mqtt = nullptr;
  modules::SensorsModule* sensors = nullptr;
  void (*open_settings)() = nullptr;
  void (*show_logs)() = nullptr;
};

class AIActions {
 public:
  void Bind(const ActionContext& ctx) { ctx_ = ctx; }
  void SetRequireConfirm(bool v) { require_confirm_ = v; }

  static const char* Name(ActionId id);
  static ActionRisk RiskOf(ActionId id);

  bool Offer(ActionId id, const char* args = nullptr);
  bool HasPending() const { return pending_.pending_confirm; }
  const ActionRequest& Pending() const { return pending_; }
  bool Confirm();
  void Cancel();

  bool Execute(ActionId id, const char* args = nullptr);

  // Parse simple intent from user text: "скан wifi", "scan nrf", etc.
  ActionId ParseIntent(const char* text) const;

 private:
  bool Run(ActionId id, const char* args);

  ActionContext ctx_;
  ActionRequest pending_;
  bool require_confirm_ = true;
};

}  // namespace axiom::ai
