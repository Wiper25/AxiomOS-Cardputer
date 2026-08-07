#pragma once

#include <stdint.h>

namespace axiom::modules {

struct BleDevice {
  char name[24] = {0};
  char addr[18] = {0};  // AA:BB:CC:DD:EE:FF
  int8_t rssi = -127;
};

struct BluetoothTelemetry {
  bool initialized = false;
  bool scanning = false;
  uint8_t devices_found = 0;
  int8_t strongest_rssi = -127;
};

class BluetoothModule {
 public:
  static constexpr uint8_t kMaxDevices = 16;

  bool Begin();
  void Tick();
  bool StartScan();
  void SetScannerActive(bool active);
  bool IsScanning() const { return telemetry_.scanning; }
  uint8_t DeviceCount() const { return device_count_; }
  const BleDevice& DeviceAt(uint8_t index) const { return devices_[index]; }
  BluetoothTelemetry GetTelemetry() const { return telemetry_; }

  // Called from NimBLE scan callback (any task).
  void OnAdvertisement(const char* addr, const char* name, int8_t rssi);
  void OnScanComplete();

 private:
  bool EnsureStack();
  void TearDownStack();
  void SortByRssi();
  void UpdateTelemetry();

  BluetoothTelemetry telemetry_;
  BleDevice devices_[kMaxDevices];
  uint8_t device_count_ = 0;
  bool scanner_active_ = false;
  bool stack_ready_ = false;
  bool scan_start_pending_ = false;
  uint32_t last_scan_start_ms_ = 0;
};

}  // namespace axiom::modules
