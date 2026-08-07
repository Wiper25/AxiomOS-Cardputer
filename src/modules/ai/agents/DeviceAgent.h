#pragma once

#include "modules/ai/AIAgent.h"
#include "modules/sensors/sensors_module.h"

namespace axiom::ai {

class DeviceAgent : public IAgent {
 public:
  explicit DeviceAgent(modules::SensorsModule* sensors) : sensors_(sensors) {}
  AgentId Id() const override { return AgentId::Device; }
  const char* Name() const override { return "Device"; }
  bool Analyze(AgentReport& out) override;

 private:
  modules::SensorsModule* sensors_;
};

}  // namespace axiom::ai
