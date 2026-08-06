#pragma once

#include <stdint.h>

namespace axiom::modules {

struct SensorTelemetry {
  bool imu_ok = false;
  const char* imu_name = "—";
  float ax = 0, ay = 0, az = 0;  // g
  float gx = 0, gy = 0, gz = 0;  // deg/s
  float imu_temp = 0;            // C from BMI270
  bool has_imu_temp = false;
  int32_t battery_percent = -1;
  int16_t battery_mv = -1;
  bool charging = false;
  uint32_t free_heap = 0;
  uint32_t min_heap = 0;
  uint32_t flash_size = 0;
};

class SensorsModule {
 public:
  bool Begin();
  void Tick();
  void SetLiveMode(bool enabled) { live_mode_ = enabled; }
  bool LiveMode() const { return live_mode_; }
  SensorTelemetry GetTelemetry() const { return tel_; }

 private:
  void EnsureImu();
  void SampleImu();
  void SampleSystem();

  SensorTelemetry tel_;
  uint32_t last_imu_ms_ = 0;
  uint32_t last_sys_ms_ = 0;
  bool live_mode_ = false;
};

}  // namespace axiom::modules
