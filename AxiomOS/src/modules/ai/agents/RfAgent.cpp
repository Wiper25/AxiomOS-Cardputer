#include "modules/ai/agents/RfAgent.h"

#include <stdio.h>
#include <string.h>

namespace axiom::ai {

bool RfAgent::Analyze(AgentReport& out) {
  out = AgentReport{};
  out.id = AgentId::Rf;
  strncpy(out.title, "RF Agent", sizeof(out.title) - 1);
  if (!nrf_) {
    out.ok = false;
    out.severity = 2;
    strncpy(out.summary, "nRF модуль null", sizeof(out.summary) - 1);
    return true;
  }
  const auto t = nrf_->GetTelemetry();
  if (!t.present) {
    out.ok = false;
    out.severity = 2;
    strncpy(out.summary, "nRF24 не обнаружен (SPI/питание)", sizeof(out.summary) - 1);
    strncpy(out.recommendation, "Проверь модуль PA+LNA и пины CE/CSN.",
            sizeof(out.recommendation) - 1);
    return true;
  }
  if (t.activity_percent >= 70) {
    out.ok = true;
    out.severity = 1;
    snprintf(out.summary, sizeof(out.summary),
             "Канал %u: активность %u%%, hot=%u, PA=%u", t.current_channel, t.activity_percent,
             t.hottest_channel, t.pa_level);
    snprintf(out.recommendation, sizeof(out.recommendation),
             "Высокие помехи. Попробуй канал вдали от %u (WiFi).", t.hottest_channel);
  } else {
    out.ok = true;
    out.severity = 0;
    snprintf(out.summary, sizeof(out.summary), "nRF ready ch=%u act=%u%% PA=%u",
             t.current_channel, t.activity_percent, t.pa_level);
    strncpy(out.recommendation, "Канал относительно свободен.",
            sizeof(out.recommendation) - 1);
  }
  return true;
}

}  // namespace axiom::ai
