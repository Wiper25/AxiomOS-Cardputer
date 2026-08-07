#pragma once

#include <stdint.h>

namespace axiom::ai {

enum class AgentId : uint8_t {
  Network = 0,
  Device,
  Rf,
  Automation,
  Update,
  Count
};

struct AgentReport {
  AgentId id = AgentId::Network;
  bool ok = true;
  char title[28] = {0};
  char summary[160] = {0};
  char recommendation[120] = {0};
  uint8_t severity = 0;  // 0 ok, 1 warn, 2 critical
};

class IAgent {
 public:
  virtual ~IAgent() = default;
  virtual AgentId Id() const = 0;
  virtual const char* Name() const = 0;
  virtual bool Analyze(AgentReport& out) = 0;
};

}  // namespace axiom::ai
