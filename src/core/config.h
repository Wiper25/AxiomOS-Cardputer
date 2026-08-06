#pragma once

#include <stdint.h>

namespace axiom {

constexpr const char* kProjectName = "AxiomOS Cardputer Edition";
constexpr const char* kProjectVersion = "0.1.0";
constexpr uint32_t kLvglTickMs = 5;
constexpr uint32_t kUiTaskPeriodMs = 5;
constexpr uint32_t kUiTaskStackWords = 6144;
constexpr uint32_t kUiTaskPriority = 3;
constexpr uint32_t kNrfTaskPeriodMs = 400;
constexpr uint32_t kNrfTaskStackWords = 4096;
constexpr uint32_t kNrfTaskPriority = 2;
constexpr uint32_t kServicesTaskPeriodMs = 500;
constexpr uint32_t kServicesTaskStackWords = 6144;
constexpr uint32_t kServicesTaskPriority = 2;

constexpr int kNrfCePin = 4;
constexpr int kNrfCsnPin = 12;
constexpr int kNrfSckPin = 40;
constexpr int kNrfMosiPin = 14;
constexpr int kNrfMisoPin = 39;

// Cardputer microSD (same SPI as EXT / nRF)
constexpr int kSdSckPin = 40;
constexpr int kSdMisoPin = 39;
constexpr int kSdMosiPin = 14;
constexpr int kSdCsPin = 12;

// Grove I2C
constexpr int kGroveSdaPin = 2;
constexpr int kGroveSclPin = 1;

}  // namespace axiom
