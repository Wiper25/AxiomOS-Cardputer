#include "modules/ai/AIActions.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modules/mqtt/mqtt_module.h"
#include "modules/nrf24/nrf24_module.h"
#include "modules/sensors/sensors_module.h"
#include "modules/wifi/wifi_module.h"

namespace axiom::ai {

const char* AIActions::Name(ActionId id) {
  switch (id) {
    case ActionId::WifiScan:
      return "wifi_scan";
    case ActionId::ScanNrf24:
      return "scan_nrf24";
    case ActionId::OpenSettings:
      return "open_settings";
    case ActionId::MqttPublish:
      return "mqtt_publish";
    case ActionId::ShowLogs:
      return "show_logs";
    case ActionId::RfSetChannel:
      return "rf_set_channel";
    case ActionId::DoctorRun:
      return "doctor_run";
    default:
      return "none";
  }
}

ActionRisk AIActions::RiskOf(ActionId id) {
  switch (id) {
    case ActionId::WifiScan:
    case ActionId::ScanNrf24:
    case ActionId::ShowLogs:
    case ActionId::DoctorRun:
    case ActionId::OpenSettings:
      return ActionRisk::Safe;
    case ActionId::MqttPublish:
    case ActionId::RfSetChannel:
      return ActionRisk::Confirm;
    default:
      return ActionRisk::Dangerous;
  }
}

bool AIActions::Offer(ActionId id, const char* args) {
  if (id == ActionId::None) return false;
  const ActionRisk risk = RiskOf(id);
  if (require_confirm_ && risk >= ActionRisk::Confirm) {
    pending_.id = id;
    pending_.risk = risk;
    pending_.pending_confirm = true;
    pending_.args[0] = 0;
    if (args) {
      strncpy(pending_.args, args, sizeof(pending_.args) - 1);
      pending_.args[sizeof(pending_.args) - 1] = 0;
    }
    return true;
  }
  return Execute(id, args);
}

bool AIActions::Confirm() {
  if (!pending_.pending_confirm) return false;
  const ActionId id = pending_.id;
  char args[kMaxActionArgs];
  strncpy(args, pending_.args, sizeof(args) - 1);
  args[sizeof(args) - 1] = 0;
  pending_ = ActionRequest{};
  return Execute(id, args);
}

void AIActions::Cancel() { pending_ = ActionRequest{}; }

bool AIActions::Execute(ActionId id, const char* args) { return Run(id, args); }

bool AIActions::Run(ActionId id, const char* args) {
  switch (id) {
    case ActionId::WifiScan:
      if (!ctx_.wifi) return false;
      ctx_.wifi->SetScannerActive(true);
      return ctx_.wifi->StartScan();
    case ActionId::ScanNrf24:
      if (!ctx_.nrf) return false;
      ctx_.nrf->SetScannerActive(true);
      return true;
    case ActionId::OpenSettings:
      if (ctx_.open_settings) {
        ctx_.open_settings();
        return true;
      }
      return false;
    case ActionId::MqttPublish:
      if (!ctx_.mqtt) return false;
      return ctx_.mqtt->Publish();
    case ActionId::ShowLogs:
      if (ctx_.show_logs) {
        ctx_.show_logs();
        return true;
      }
      return false;
    case ActionId::RfSetChannel: {
      if (!ctx_.nrf || !args) return false;
      const int ch = atoi(args);
      if (ch < 0 || ch > 125) return false;
      return ctx_.nrf->SetChannel(static_cast<uint8_t>(ch));
    }
    case ActionId::DoctorRun:
      return true;
    default:
      return false;
  }
}

ActionId AIActions::ParseIntent(const char* text) const {
  if (!text) return ActionId::None;
  char q[96];
  strncpy(q, text, sizeof(q) - 1);
  q[sizeof(q) - 1] = 0;
  for (char* p = q; *p; ++p) *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));

  if (strstr(q, "wifi") && (strstr(q, "scan") || strstr(q, "скан"))) return ActionId::WifiScan;
  if ((strstr(q, "nrf") || strstr(q, "спектр") || strstr(q, "spectrum")) &&
      (strstr(q, "scan") || strstr(q, "скан") || strstr(q, "анализ"))) {
    return ActionId::ScanNrf24;
  }
  if (strstr(q, "mqtt") && (strstr(q, "pub") || strstr(q, "отправ"))) return ActionId::MqttPublish;
  if (strstr(q, "настрой") || strstr(q, "settings")) return ActionId::OpenSettings;
  if (strstr(q, "doctor") || strstr(q, "диагност") || strstr(q, "doctor")) return ActionId::DoctorRun;
  if (strstr(q, "лог")) return ActionId::ShowLogs;
  return ActionId::None;
}

}  // namespace axiom::ai
