#pragma once

#include <string.h>

#include "modules/ai/AIAgent.h"

namespace axiom::ai {

class UpdateAgent : public IAgent {
 public:
  AgentId Id() const override { return AgentId::Update; }
  const char* Name() const override { return "Update"; }
  bool Analyze(AgentReport& out) override;
  void SetLatestRemote(const char* ver) {
    if (ver) {
      strncpy(remote_ver_, ver, sizeof(remote_ver_) - 1);
      remote_ver_[sizeof(remote_ver_) - 1] = 0;
      checked_ = true;
    }
  }

 private:
  char remote_ver_[16] = {0};
  bool checked_ = false;
};

}  // namespace axiom::ai
