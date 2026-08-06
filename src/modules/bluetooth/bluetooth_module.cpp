#include "modules/bluetooth/bluetooth_module.h"

namespace axiom::modules {

bool BluetoothModule::Begin() {
  telemetry_.initialized = true;
  return true;
}

}  // namespace axiom::modules
