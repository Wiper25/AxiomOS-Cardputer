#pragma once

namespace axiom::modules {

struct BluetoothTelemetry {
  bool initialized = false;
};

class BluetoothModule {
 public:
  bool Begin();
  void Tick() {}
  BluetoothTelemetry GetTelemetry() const { return telemetry_; }

 private:
  BluetoothTelemetry telemetry_;
};

}  // namespace axiom::modules
