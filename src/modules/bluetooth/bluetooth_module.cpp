#include "modules/bluetooth/bluetooth_module.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

namespace axiom::modules {
namespace {

constexpr uint32_t kScanDurationSec = 4;
constexpr uint16_t kScanInterval = 100;  // units of 0.625ms
constexpr uint16_t kScanWindow = 99;

BluetoothModule* g_bt = nullptr;

class AdvCallbacks : public NimBLEAdvertisedDeviceCallbacks {
 public:
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
    if (g_bt == nullptr || advertisedDevice == nullptr) return;

    const std::string addr = advertisedDevice->getAddress().toString();
    char name_buf[24] = {0};
    if (advertisedDevice->haveName()) {
      const std::string n = advertisedDevice->getName();
      strncpy(name_buf, n.c_str(), sizeof(name_buf) - 1);
    }
    g_bt->OnAdvertisement(addr.c_str(), name_buf[0] ? name_buf : nullptr,
                          static_cast<int8_t>(advertisedDevice->getRSSI()));
  }
};

void OnScanDone(NimBLEScanResults results) {
  (void)results;
  if (g_bt) g_bt->OnScanComplete();
}

AdvCallbacks g_adv_callbacks;

}  // namespace

bool BluetoothModule::Begin() {
  g_bt = this;
  telemetry_ = BluetoothTelemetry{};
  device_count_ = 0;
  scanner_active_ = false;
  stack_ready_ = false;
  scan_start_pending_ = false;
  return true;
}

bool BluetoothModule::EnsureStack() {
  if (stack_ready_) return true;

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan == nullptr) return false;

  scan->setAdvertisedDeviceCallbacks(&g_adv_callbacks, true);
  scan->setActiveScan(true);
  scan->setInterval(kScanInterval);
  scan->setWindow(kScanWindow);
  scan->setDuplicateFilter(false);
  scan->setMaxResults(0);  // don't retain in NimBLE — we keep our own list

  stack_ready_ = true;
  telemetry_.initialized = true;
  return true;
}

void BluetoothModule::TearDownStack() {
  if (!stack_ready_) return;

  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan != nullptr && scan->isScanning()) {
    scan->stop();
  }
  NimBLEDevice::deinit(true);
  stack_ready_ = false;
  telemetry_.initialized = false;
  telemetry_.scanning = false;
}

bool BluetoothModule::StartScan() {
  if (!scanner_active_) return false;
  if (!EnsureStack()) return false;

  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan == nullptr) return false;
  if (scan->isScanning()) return true;

  device_count_ = 0;
  telemetry_.devices_found = 0;
  telemetry_.strongest_rssi = -127;
  telemetry_.scanning = true;
  last_scan_start_ms_ = millis();

  const bool ok = scan->start(kScanDurationSec, OnScanDone, false);
  if (!ok) {
    telemetry_.scanning = false;
    return false;
  }
  return true;
}

void BluetoothModule::SetScannerActive(bool active) {
  scanner_active_ = active;
  if (active) {
    scan_start_pending_ = true;
    return;
  }

  scan_start_pending_ = false;
  if (stack_ready_) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan != nullptr && scan->isScanning()) {
      scan->stop();
    }
  }
  telemetry_.scanning = false;
  TearDownStack();
}

void BluetoothModule::OnAdvertisement(const char* addr, const char* name, int8_t rssi) {
  if (addr == nullptr || addr[0] == '\0') return;

  for (uint8_t i = 0; i < device_count_; ++i) {
    if (strcasecmp(devices_[i].addr, addr) == 0) {
      devices_[i].rssi = rssi;
      if (name && name[0]) {
        strncpy(devices_[i].name, name, sizeof(devices_[i].name) - 1);
        devices_[i].name[sizeof(devices_[i].name) - 1] = 0;
      }
      SortByRssi();
      UpdateTelemetry();
      return;
    }
  }

  if (device_count_ >= kMaxDevices) {
    uint8_t weakest = 0;
    for (uint8_t i = 1; i < device_count_; ++i) {
      if (devices_[i].rssi < devices_[weakest].rssi) weakest = i;
    }
    if (rssi <= devices_[weakest].rssi) return;
    strncpy(devices_[weakest].addr, addr, sizeof(devices_[weakest].addr) - 1);
    devices_[weakest].addr[sizeof(devices_[weakest].addr) - 1] = 0;
    devices_[weakest].name[0] = 0;
    if (name && name[0]) {
      strncpy(devices_[weakest].name, name, sizeof(devices_[weakest].name) - 1);
      devices_[weakest].name[sizeof(devices_[weakest].name) - 1] = 0;
    }
    devices_[weakest].rssi = rssi;
  } else {
    BleDevice& d = devices_[device_count_++];
    strncpy(d.addr, addr, sizeof(d.addr) - 1);
    d.addr[sizeof(d.addr) - 1] = 0;
    d.name[0] = 0;
    if (name && name[0]) {
      strncpy(d.name, name, sizeof(d.name) - 1);
      d.name[sizeof(d.name) - 1] = 0;
    }
    d.rssi = rssi;
  }

  SortByRssi();
  UpdateTelemetry();
}

void BluetoothModule::OnScanComplete() {
  telemetry_.scanning = false;
  UpdateTelemetry();
  if (scanner_active_) {
    scan_start_pending_ = true;
  }
}

void BluetoothModule::Tick() {
  if (scan_start_pending_ && scanner_active_) {
    if (millis() - last_scan_start_ms_ < 400 && last_scan_start_ms_ != 0 && device_count_ > 0) {
      return;
    }
    scan_start_pending_ = false;
    StartScan();
  }
}

void BluetoothModule::SortByRssi() {
  for (uint8_t i = 1; i < device_count_; ++i) {
    BleDevice key = devices_[i];
    int j = static_cast<int>(i) - 1;
    while (j >= 0 && devices_[j].rssi < key.rssi) {
      devices_[j + 1] = devices_[j];
      --j;
    }
    devices_[j + 1] = key;
  }
}

void BluetoothModule::UpdateTelemetry() {
  telemetry_.devices_found = device_count_;
  telemetry_.strongest_rssi = device_count_ > 0 ? devices_[0].rssi : static_cast<int8_t>(-127);
}

}  // namespace axiom::modules
