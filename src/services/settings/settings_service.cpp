#include "services/settings/settings_service.h"

#include <Preferences.h>

namespace axiom::services {

bool SettingsService::Begin() { return true; }

bool SettingsService::Load(AppSettings& out) {
  Preferences prefs;
  if (!prefs.begin(kNs, true)) {
    return false;
  }
  out.brightness = prefs.getUChar("brightness", out.brightness);
  out.volume = prefs.getUChar("volume", out.volume);
  out.theme = static_cast<ThemeMode>(prefs.getUChar("theme", static_cast<uint8_t>(out.theme)));
  out.rf_channel = prefs.getUChar("rf_ch", out.rf_channel);
  out.rf_power = prefs.getUChar("rf_pwr", out.rf_power);
  out.last_section = prefs.getUChar("last_sec", out.last_section);
  prefs.end();
  return true;
}

bool SettingsService::Save(const AppSettings& in) {
  Preferences prefs;
  if (!prefs.begin(kNs, false)) {
    return false;
  }
  prefs.putUChar("brightness", in.brightness);
  prefs.putUChar("volume", in.volume);
  prefs.putUChar("theme", static_cast<uint8_t>(in.theme));
  prefs.putUChar("rf_ch", in.rf_channel);
  prefs.putUChar("rf_pwr", in.rf_power);
  prefs.putUChar("last_sec", in.last_section);
  prefs.end();
  return true;
}

}  // namespace axiom::services
