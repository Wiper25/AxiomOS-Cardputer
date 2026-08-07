#pragma once

#include <stddef.h>
#include <stdint.h>

#include "modules/ai/AIAgent.h"

namespace axiom {
namespace modules {
class WifiModule;
class Nrf24Module;
class SensorsModule;
class MqttModule;
}  // namespace modules
}  // namespace axiom

namespace axiom::ai {

struct DoctorFinding {
  uint8_t severity = 0;
  char problem[64] = {0};
  char cause[96] = {0};
  char fix[96] = {0};
};

class AIDoctor {
 public:
  void Bind(modules::WifiModule* wifi, modules::Nrf24Module* nrf,
            modules::SensorsModule* sensors, modules::MqttModule* mqtt) {
    wifi_ = wifi;
    nrf_ = nrf;
    sensors_ = sensors;
    mqtt_ = mqtt;
  }

  bool Diagnose(DoctorFinding* out, uint8_t max_out, uint8_t& count);
  bool DiagnoseText(char* dst, size_t n);

 private:
  modules::WifiModule* wifi_ = nullptr;
  modules::Nrf24Module* nrf_ = nullptr;
  modules::SensorsModule* sensors_ = nullptr;
  modules::MqttModule* mqtt_ = nullptr;
};

}  // namespace axiom::ai
