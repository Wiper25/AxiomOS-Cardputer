#include "modules/sensors/sensors_module.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>

namespace axiom::modules {

namespace {
const char* ImuName(m5::imu_t t) {
  switch (t) {
    case m5::imu_t::imu_bmi270:
      return "BMI270";
    case m5::imu_t::imu_mpu6886:
      return "MPU6886";
    case m5::imu_t::imu_mpu6050:
      return "MPU6050";
    case m5::imu_t::imu_mpu9250:
      return "MPU9250";
    case m5::imu_t::imu_sh200q:
      return "SH200Q";
    default:
      return "нет";
  }
}

float Sanitize(float v) {
  if (!isfinite(v)) return 0.0f;
  if (v > 9999.0f) return 9999.0f;
  if (v < -9999.0f) return -9999.0f;
  return v;
}
}  // namespace

void SensorsModule::EnsureImu() {
  if (M5.Imu.isEnabled()) return;
  M5.Imu.begin(&M5.In_I2C, M5.getBoard());
  if (!M5.Imu.isEnabled() && M5.Ex_I2C.isEnabled()) {
    M5.Imu.begin(&M5.Ex_I2C, M5.getBoard());
  }
}

void SensorsModule::SampleImu() {
  EnsureImu();
  tel_.imu_ok = M5.Imu.isEnabled();
  tel_.imu_name = ImuName(M5.Imu.getType());
  if (!tel_.imu_ok) {
    tel_.has_imu_temp = false;
    return;
  }

  M5.Imu.update();
  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  M5.Imu.getAccel(&ax, &ay, &az);
  M5.Imu.getGyro(&gx, &gy, &gz);
  tel_.ax = Sanitize(ax);
  tel_.ay = Sanitize(ay);
  tel_.az = Sanitize(az);
  tel_.gx = Sanitize(gx);
  tel_.gy = Sanitize(gy);
  tel_.gz = Sanitize(gz);

  float t = 0;
  tel_.has_imu_temp = M5.Imu.getTemp(&t);
  tel_.imu_temp = tel_.has_imu_temp ? Sanitize(t) : 0;
}

void SensorsModule::SampleSystem() {
  tel_.free_heap = ESP.getFreeHeap();
  if (tel_.free_heap < tel_.min_heap) tel_.min_heap = tel_.free_heap;
  tel_.flash_size = ESP.getFlashChipSize();
  const int32_t level = M5.Power.getBatteryLevel();
  tel_.battery_percent = (level >= 0 && level <= 100) ? level : -1;
  tel_.battery_mv = M5.Power.getBatteryVoltage();
  tel_.charging = M5.Power.isCharging();
}

bool SensorsModule::Begin() {
  tel_.flash_size = ESP.getFlashChipSize();
  tel_.min_heap = ESP.getFreeHeap();
  EnsureImu();
  SampleImu();
  SampleSystem();
  return true;
}

void SensorsModule::Tick() {
  const uint32_t now = millis();
  const uint32_t imu_period = live_mode_ ? 8U : 200U;
  if (now - last_imu_ms_ >= imu_period) {
    last_imu_ms_ = now;
    SampleImu();
  }
  if (now - last_sys_ms_ >= 400U) {
    last_sys_ms_ = now;
    SampleSystem();
  }
}

}  // namespace axiom::modules
