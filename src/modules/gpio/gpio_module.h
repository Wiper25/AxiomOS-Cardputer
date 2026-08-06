#pragma once

#include <stdint.h>

namespace axiom::modules {

struct GpioPinInfo {
  uint8_t pin = 0;
  bool output = false;
  bool level = false;
  bool reserved = false;
  char tag[10] = {0};
};

class GpioModule {
 public:
  static constexpr uint8_t kCount = 8;

  bool Begin();
  void Tick();
  void Refresh();
  uint8_t Count() const { return kCount; }
  const GpioPinInfo& At(uint8_t i) const { return pins_[i]; }
  bool ToggleOutput(uint8_t index);
  bool SetOutput(uint8_t index, bool high);

 private:
  GpioPinInfo pins_[kCount];
};

}  // namespace axiom::modules
