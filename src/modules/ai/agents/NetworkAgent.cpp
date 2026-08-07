#include "modules/ai/agents/NetworkAgent.h"

#include <stdio.h>
#include <string.h>

namespace axiom::ai {

bool NetworkAgent::Analyze(AgentReport& out) {
  out = AgentReport{};
  out.id = AgentId::Network;
  strncpy(out.title, "Network Agent", sizeof(out.title) - 1);
  if (!wifi_) {
    out.ok = false;
    out.severity = 2;
    strncpy(out.summary, "WiFi модуль недоступен", sizeof(out.summary) - 1);
    return true;
  }
  const auto t = wifi_->GetTelemetry();
  if (!t.connected) {
    out.ok = false;
    out.severity = 1;
    snprintf(out.summary, sizeof(out.summary),
             "STA offline. Скан: %d сетей, лучший RSSI %d", static_cast<int>(t.networks_found),
             static_cast<int>(t.strongest_rssi));
    strncpy(out.recommendation, "Открой Сканер WiFi и подключись к AP.",
            sizeof(out.recommendation) - 1);
    return true;
  }
  if (t.link_rssi < -75) {
    out.ok = true;
    out.severity = 1;
    snprintf(out.summary, sizeof(out.summary), "Подключено к %s, RSSI %d (слабо)",
             t.connected_ssid, static_cast<int>(t.link_rssi));
    strncpy(out.recommendation, "Подойди ближе к AP или смени канал роутера.",
            sizeof(out.recommendation) - 1);
  } else {
    out.ok = true;
    out.severity = 0;
    snprintf(out.summary, sizeof(out.summary), "OK: %s RSSI %d", t.connected_ssid,
             static_cast<int>(t.link_rssi));
    strncpy(out.recommendation, "Канал стабилен для MQTT/AI клиента.",
            sizeof(out.recommendation) - 1);
  }
  return true;
}

}  // namespace axiom::ai
