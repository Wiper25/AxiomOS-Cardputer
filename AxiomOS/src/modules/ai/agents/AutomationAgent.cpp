#include "modules/ai/agents/AutomationAgent.h"

#include <stdio.h>
#include <string.h>

namespace axiom::ai {

bool AutomationAgent::Analyze(AgentReport& out) {
  out = AgentReport{};
  out.id = AgentId::Automation;
  strncpy(out.title, "Automation", sizeof(out.title) - 1);
  const bool wifi_ok = wifi_ && wifi_->GetTelemetry().connected;
  if (!wifi_ok) {
    out.ok = false;
    out.severity = 1;
    strncpy(out.summary, "MQTT/IoT сценарии требуют WiFi", sizeof(out.summary) - 1);
    strncpy(out.recommendation, "Сначала подключи WiFi, затем MQTT Connect.",
            sizeof(out.recommendation) - 1);
    return true;
  }
  if (!mqtt_) {
    out.ok = false;
    out.severity = 2;
    strncpy(out.summary, "MQTT модуль недоступен", sizeof(out.summary) - 1);
    return true;
  }
  const auto t = mqtt_->GetTelemetry();
  const auto& cfg = mqtt_->Config();
  if (t.state != modules::MqttState::Connected) {
    out.ok = true;
    out.severity = 1;
    snprintf(out.summary, sizeof(out.summary), "MQTT не подключён к %s:%u", cfg.host, cfg.port);
    strncpy(out.recommendation, "Открой MQTT клиент → Connect.",
            sizeof(out.recommendation) - 1);
  } else {
    out.ok = true;
    out.severity = 0;
    snprintf(out.summary, sizeof(out.summary), "MQTT OK pub=%s sub=%s", cfg.pub_topic,
             cfg.sub_topic);
    strncpy(out.recommendation, "Можно публиковать сценарии IoT через AI Actions.",
            sizeof(out.recommendation) - 1);
  }
  return true;
}

}  // namespace axiom::ai
