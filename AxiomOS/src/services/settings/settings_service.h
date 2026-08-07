#pragma once

#include <stdint.h>

namespace axiom::services {

enum class ThemeMode : uint8_t { Dark = 0, Cyberpunk = 1 };

struct AppSettings {
  uint8_t brightness = 180;
  uint8_t volume = 96;
  ThemeMode theme = ThemeMode::Cyberpunk;
  uint8_t rf_channel = 76;
  uint8_t rf_power = 3;
  uint8_t last_section = 0;
};

class SettingsService {
 public:
  bool Begin();
  bool Load(AppSettings& out);
  bool Save(const AppSettings& in);

 private:
  static constexpr const char* kNs = "axiomos";
};

}  // namespace axiom::services
