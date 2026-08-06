#include "modules/i2c/i2c_module.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "core/config.h"

namespace axiom::modules {

namespace {
constexpr uint8_t kScanFirst = 0x08;
constexpr uint8_t kScanLast = 0x78;
constexpr int kAddrsPerTick = 8;
constexpr uint16_t kI2cTimeoutMs = 20;
}  // namespace

bool I2cModule::Begin() {
  count_ = 0;
  scanning_ = false;
  next_addr_ = kScanFirst;
  return true;
}

const char* I2cModule::GuessName(uint8_t addr) {
  switch (addr) {
    case 0x18:
      return "ES8311";
    case 0x34:
      return "TCA8418";
    case 0x68:
    case 0x69:
      return "BMI270";
    case 0x3C:
    case 0x3D:
      return "OLED?";
    case 0x76:
    case 0x77:
      return "BME/BMP";
    default:
      return "";
  }
}

void I2cModule::PrepareBus() {
  if (bus_ == I2cBusId::Grove) {
    Wire1.end();
    Wire1.begin(kGroveSdaPin, kGroveSclPin, 100000);
    Wire1.setTimeOut(kI2cTimeoutMs);
  } else {
    // Internal bus already owned by M5 — only clamp timeout
    Wire.setTimeOut(kI2cTimeoutMs);
  }
}

bool I2cModule::StartScan(I2cBusId bus) {
  bus_ = bus;
  count_ = 0;
  next_addr_ = kScanFirst;
  scanning_ = true;
  PrepareBus();
  return true;
}

uint8_t I2cModule::ProgressPercent() const {
  if (!scanning_) return 100;
  const uint16_t done = static_cast<uint16_t>(next_addr_ - kScanFirst);
  const uint16_t total = static_cast<uint16_t>(kScanLast - kScanFirst);
  return static_cast<uint8_t>((done * 100U) / total);
}

void I2cModule::Tick() {
  if (!scanning_) return;

  TwoWire* wire = (bus_ == I2cBusId::Grove) ? &Wire1 : &Wire;
  wire->setTimeOut(kI2cTimeoutMs);

  for (int n = 0; n < kAddrsPerTick && next_addr_ < kScanLast; ++n, ++next_addr_) {
    wire->beginTransmission(next_addr_);
    const uint8_t err = wire->endTransmission(true);
    if (err == 0 && count_ < kMaxDevices) {
      devices_[count_].addr = next_addr_;
      strncpy(devices_[count_].name, GuessName(next_addr_), sizeof(devices_[count_].name) - 1);
      devices_[count_].name[sizeof(devices_[count_].name) - 1] = '\0';
      ++count_;
    }
  }

  if (next_addr_ >= kScanLast) {
    scanning_ = false;
  }
}

}  // namespace axiom::modules
