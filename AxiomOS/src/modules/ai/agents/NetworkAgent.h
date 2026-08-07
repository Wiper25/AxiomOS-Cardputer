#pragma once

#include "modules/ai/AIAgent.h"
#include "modules/wifi/wifi_module.h"

namespace axiom::ai {

class NetworkAgent : public IAgent {
 public:
  explicit NetworkAgent(modules::WifiModule* wifi) : wifi_(wifi) {}
  AgentId Id() const override { return AgentId::Network; }
  const char* Name() const override { return "Network"; }
  bool Analyze(AgentReport& out) override;

 private:
  modules::WifiModule* wifi_;
};

}  // namespace axiom::ai
