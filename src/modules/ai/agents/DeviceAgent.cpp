#include "modules/ai/agents/DeviceAgent.h"

#include <stdio.h>
#include <string.h>

namespace axiom::ai {

bool DeviceAgent::Analyze(AgentReport& out) {
  out = AgentReport{};
  out.id = AgentId::Device;
  strncpy(out.title, "Device Agent", sizeof(out.title) - 1);
  if (!sensors_) {
    out.ok = false;
    out.severity = 2;
    strncpy(out.summary, "Sensors недоступны", sizeof(out.summary) - 1);
    return true;
  }
  const auto t = sensors_->GetTelemetry();
  if (t.free_heap < 40000U) {
    out.ok = false;
    out.severity = 2;
    snprintf(out.summary, sizeof(out.summary), "Мало RAM: %lu байт, min %lu",
             static_cast<unsigned long>(t.free_heap), static_cast<unsigned long>(t.min_heap));
    strncpy(out.recommendation, "Закрой тяжёлые экраны, перезагрузи при утечке.",
            sizeof(out.recommendation) - 1);
  } else if (t.battery_percent >= 0 && t.battery_percent < 15 && !t.charging) {
    out.ok = true;
    out.severity = 1;
    snprintf(out.summary, sizeof(out.summary), "Батарея %ld%%, %dmV",
             static_cast<long>(t.battery_percent), static_cast<int>(t.battery_mv));
    strncpy(out.recommendation, "Подключи питание — риск reboot при TX nRF.",
            sizeof(out.recommendation) - 1);
  } else {
    out.ok = true;
    out.severity = 0;
    snprintf(out.summary, sizeof(out.summary), "Heap %lu, bat %ld%%, IMU %s",
             static_cast<unsigned long>(t.free_heap), static_cast<long>(t.battery_percent),
             t.imu_ok ? t.imu_name : "n/a");
    strncpy(out.recommendation, "Железо в норме.", sizeof(out.recommendation) - 1);
  }
  return true;
}

}  // namespace axiom::ai
