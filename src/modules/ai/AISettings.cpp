#include "modules/ai/AISettings.h"

#include <Preferences.h>
#include <string.h>

namespace axiom::ai {

void AISettings::ApplyApiFromSource() {
  // Endpoint / model / key — всегда из ai_config.h (не из NVS, не из UI)
  memset(data_.rest_host, 0, sizeof(data_.rest_host));
  memset(data_.rest_path, 0, sizeof(data_.rest_path));
  memset(data_.ws_host, 0, sizeof(data_.ws_host));
  memset(data_.ws_path, 0, sizeof(data_.ws_path));
  memset(data_.model, 0, sizeof(data_.model));
  memset(data_.api_key, 0, sizeof(data_.api_key));

  strncpy(data_.rest_host, kDefaultRestHost, sizeof(data_.rest_host) - 1);
  strncpy(data_.rest_path, kDefaultRestPath, sizeof(data_.rest_path) - 1);
  strncpy(data_.ws_host, kDefaultWsHost, sizeof(data_.ws_host) - 1);
  strncpy(data_.ws_path, kDefaultWsPath, sizeof(data_.ws_path) - 1);
  strncpy(data_.model, kDefaultModel, sizeof(data_.model) - 1);
  strncpy(data_.api_key, kDefaultApiKey, sizeof(data_.api_key) - 1);
  data_.rest_port = kDefaultRestPort;
  data_.ws_port = kDefaultWsPort;
  data_.use_https = kDefaultUseHttps;
}

bool AISettings::Begin() {
  data_.enabled = true;
  data_.local_only = false;
  data_.stream = kDefaultStream;
  data_.confirm_actions = true;
  data_.voice_ready = false;
  data_.transport = static_cast<AiTransport>(kDefaultTransport);
  data_.temperature_x10 = 7;
  ApplyApiFromSource();
  Load();
  ApplyApiFromSource();  // NVS never overrides API endpoint/key
  return true;
}

bool AISettings::Load() {
  Preferences prefs;
  if (!prefs.begin(kAiPrefsNs, true)) {
    return false;
  }
  data_.enabled = prefs.getBool("en", true);
  data_.local_only = prefs.getBool("local", false);
  data_.stream = prefs.getBool("stream", kDefaultStream);
  data_.confirm_actions = prefs.getBool("confirm", true);
  data_.voice_ready = prefs.getBool("voice", false);
  data_.transport =
      static_cast<AiTransport>(prefs.getUChar("tr", kDefaultTransport));
  data_.temperature_x10 = prefs.getUChar("temp", 7);
  prefs.end();
  dirty_ = false;
  return true;
}

bool AISettings::Save() const {
  Preferences prefs;
  if (!prefs.begin(kAiPrefsNs, false)) {
    return false;
  }
  prefs.putBool("en", data_.enabled);
  prefs.putBool("local", data_.local_only);
  prefs.putBool("stream", data_.stream);
  prefs.putBool("confirm", data_.confirm_actions);
  prefs.putBool("voice", data_.voice_ready);
  prefs.putUChar("tr", static_cast<uint8_t>(data_.transport));
  prefs.putUChar("temp", data_.temperature_x10);
  prefs.end();
  return true;
}

void AISettings::SetEnabled(bool v) {
  data_.enabled = v;
  dirty_ = true;
  Save();
}

void AISettings::SetLocalOnly(bool v) {
  data_.local_only = v;
  Save();
}

void AISettings::SetTransport(AiTransport t) {
  data_.transport = t;
  Save();
}

void AISettings::CycleTransport() {
  data_.transport = (data_.transport == AiTransport::Rest) ? AiTransport::WebSocket
                                                           : AiTransport::Rest;
  Save();
}

void AISettings::SetStream(bool v) {
  data_.stream = v;
  Save();
}

void AISettings::SetConfirmActions(bool v) {
  data_.confirm_actions = v;
  Save();
}

}  // namespace axiom::ai
