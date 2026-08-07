#include "modules/ai/agents/UpdateAgent.h"

#include <stdio.h>
#include <string.h>

#include "core/config.h"

namespace axiom::ai {

bool UpdateAgent::Analyze(AgentReport& out) {
  out = AgentReport{};
  out.id = AgentId::Update;
  strncpy(out.title, "Update Agent", sizeof(out.title) - 1);
  snprintf(out.summary, sizeof(out.summary), "Локальная версия AxiomOS %s", kProjectVersion);
  if (!checked_ || remote_ver_[0] == 0) {
    out.ok = true;
    out.severity = 0;
    strncpy(out.recommendation,
            "OTA: положи firmware.bin на SD или укажи HTTP URL в AI Settings (позже).",
            sizeof(out.recommendation) - 1);
  } else if (strcmp(remote_ver_, kProjectVersion) != 0) {
    out.ok = true;
    out.severity = 1;
    snprintf(out.summary, sizeof(out.summary), "Доступна %s (сейчас %s)", remote_ver_,
             kProjectVersion);
    strncpy(out.recommendation, "Подтверди OTA через AI Actions (опасная операция).",
            sizeof(out.recommendation) - 1);
  } else {
    out.ok = true;
    out.severity = 0;
    strncpy(out.recommendation, "Версия актуальна.", sizeof(out.recommendation) - 1);
  }
  return true;
}

}  // namespace axiom::ai
