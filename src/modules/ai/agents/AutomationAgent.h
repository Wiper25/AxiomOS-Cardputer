#pragma once

#include "modules/ai/AIAgent.h"
#include "modules/mqtt/mqtt_module.h"
#include "modules/wifi/wifi_module.h"

namespace axiom::ai {

class AutomationAgent : public IAgent {
 public:
  AutomationAgent(modules::MqttModule* mqtt, modules::WifiModule* wifi)
      : mqtt_(mqtt), wifi_(wifi) {}
  AgentId Id() const override { return AgentId::Automation; }
  const char* Name() const override { return "Automation"; }
  bool Analyze(AgentReport& out) override;

 private:
  modules::MqttModule* mqtt_;
  modules::WifiModule* wifi_;
};

}  // namespace axiom::ai
