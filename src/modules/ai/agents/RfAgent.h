#pragma once

#include "modules/ai/AIAgent.h"
#include "modules/nrf24/nrf24_module.h"

namespace axiom::ai {

class RfAgent : public IAgent {
 public:
  explicit RfAgent(modules::Nrf24Module* nrf) : nrf_(nrf) {}
  AgentId Id() const override { return AgentId::Rf; }
  const char* Name() const override { return "RF"; }
  bool Analyze(AgentReport& out) override;

 private:
  modules::Nrf24Module* nrf_;
};

}  // namespace axiom::ai
