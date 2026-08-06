#include "modules/gpio/gpio_module.h"

#include <Arduino.h>
#include <string.h>

namespace axiom::modules {

namespace {
struct PinDef {
  uint8_t pin;
  bool reserved;
  const char* tag;
};

// Safe-ish EXT/Grove pins. Reserved = read-only (SPI/I2C/nRF/SD shared).
constexpr PinDef kPins[] = {
    {1, false, "Гров"},
    {2, false, "Гров"},
    {3, false, "Разъём"},
    {5, false, "Разъём"},
    {6, false, "Разъём"},
    {13, false, "UART"},
    {15, false, "UART"},
    {4, true, "nRF CE"},
};
}  // namespace

bool GpioModule::Begin() {
  for (uint8_t i = 0; i < kCount; ++i) {
    pins_[i].pin = kPins[i].pin;
    pins_[i].reserved = kPins[i].reserved;
    pins_[i].output = false;
    strncpy(pins_[i].tag, kPins[i].tag, sizeof(pins_[i].tag) - 1);
    pinMode(pins_[i].pin, INPUT);
  }
  Refresh();
  return true;
}

void GpioModule::Tick() { Refresh(); }

void GpioModule::Refresh() {
  for (uint8_t i = 0; i < kCount; ++i) {
    pins_[i].level = digitalRead(pins_[i].pin) == HIGH;
  }
}

bool GpioModule::ToggleOutput(uint8_t index) {
  if (index >= kCount || pins_[index].reserved) return false;
  if (!pins_[index].output) {
    pinMode(pins_[index].pin, OUTPUT);
    pins_[index].output = true;
    digitalWrite(pins_[index].pin, LOW);
    pins_[index].level = false;
    return true;
  }
  const bool next = !pins_[index].level;
  digitalWrite(pins_[index].pin, next ? HIGH : LOW);
  pins_[index].level = next;
  return true;
}

bool GpioModule::SetOutput(uint8_t index, bool high) {
  if (index >= kCount || pins_[index].reserved) return false;
  pinMode(pins_[index].pin, OUTPUT);
  pins_[index].output = true;
  digitalWrite(pins_[index].pin, high ? HIGH : LOW);
  pins_[index].level = high;
  return true;
}

}  // namespace axiom::modules
