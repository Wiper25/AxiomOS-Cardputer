#pragma once

#include <stdint.h>

namespace axiom::modules {

enum class I2cBusId : uint8_t { Internal = 0, Grove = 1 };

struct I2cDevice {
  uint8_t addr = 0;
  char name[18] = {0};
};

class I2cModule {
 public:
  static constexpr uint8_t kMaxDevices = 16;

  bool Begin();
  void Tick();
  bool StartScan(I2cBusId bus);
  I2cBusId Bus() const { return bus_; }
  uint8_t Count() const { return count_; }
  const I2cDevice& At(uint8_t i) const { return devices_[i]; }
  bool Scanning() const { return scanning_; }
  uint8_t ProgressPercent() const;

 private:
  static const char* GuessName(uint8_t addr);
  void PrepareBus();

  I2cBusId bus_ = I2cBusId::Internal;
  I2cDevice devices_[kMaxDevices];
  uint8_t count_ = 0;
  bool scanning_ = false;
  uint8_t next_addr_ = 0x08;
};

}  // namespace axiom::modules
