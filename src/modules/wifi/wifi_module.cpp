#include "modules/wifi/wifi_module.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <string.h>

namespace axiom::modules {
namespace {
constexpr const char* kWifiPrefsNs = "axiom_wifi";
}

bool WifiModule::LoadCredentials() {
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNs, true)) return false;
  auto_connect_ = prefs.getBool("auto", true);
  const String ssid = prefs.getString("ssid", "");
  const String pass = prefs.getString("pass", "");
  prefs.end();

  saved_ssid_[0] = 0;
  saved_pass_[0] = 0;
  if (ssid.length() == 0 || ssid.length() >= sizeof(saved_ssid_)) {
    telemetry_.has_saved = false;
    return false;
  }
  strncpy(saved_ssid_, ssid.c_str(), sizeof(saved_ssid_) - 1);
  strncpy(saved_pass_, pass.c_str(), sizeof(saved_pass_) - 1);
  saved_ssid_[sizeof(saved_ssid_) - 1] = 0;
  saved_pass_[sizeof(saved_pass_) - 1] = 0;
  telemetry_.has_saved = true;
  telemetry_.auto_connect = auto_connect_;
  return true;
}

bool WifiModule::SaveCredentials(const char* ssid, const char* password) {
  if (!ssid || !ssid[0]) return false;
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNs, false)) return false;
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password ? password : "");
  prefs.putBool("auto", auto_connect_);
  prefs.end();

  strncpy(saved_ssid_, ssid, sizeof(saved_ssid_) - 1);
  saved_ssid_[sizeof(saved_ssid_) - 1] = 0;
  strncpy(saved_pass_, password ? password : "", sizeof(saved_pass_) - 1);
  saved_pass_[sizeof(saved_pass_) - 1] = 0;
  telemetry_.has_saved = true;
  return true;
}

void WifiModule::ForgetSaved() {
  Preferences prefs;
  if (prefs.begin(kWifiPrefsNs, false)) {
    prefs.clear();
    prefs.end();
  }
  saved_ssid_[0] = 0;
  saved_pass_[0] = 0;
  pending_save_ = false;
  telemetry_.has_saved = false;
}

void WifiModule::TryAutoConnect() {
  if (!auto_connect_ || saved_ssid_[0] == 0) return;
  ConnectSaved();
}

bool WifiModule::ConnectSaved() {
  if (saved_ssid_[0] == 0) return false;
  const bool ok = Connect(saved_ssid_, saved_pass_);
  // Already in NVS — don't rewrite
  pending_save_ = false;
  return ok;
}

bool WifiModule::Begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // Do NOT erase flash creds: disconnect(wifioff, eraseap=false)
  WiFi.disconnect(false, false);
  delay(30);

  telemetry_.station_mode = true;
  telemetry_.connected = false;
  telemetry_.scanning = false;
  telemetry_.connect_state = WifiConnectState::Idle;
  telemetry_.connected_ssid[0] = '\0';
  network_count_ = 0;
  pending_save_ = false;

  LoadCredentials();
  TryAutoConnect();
  return true;
}

bool WifiModule::StartScan() {
  if (telemetry_.scanning) return true;
  if (telemetry_.connect_state == WifiConnectState::Connecting) return false;

  WiFi.scanDelete();
  const int16_t rc = WiFi.scanNetworks(true, true);
  if (rc == WIFI_SCAN_FAILED) {
    telemetry_.scanning = false;
    return false;
  }
  telemetry_.scanning = true;
  return true;
}

void WifiModule::SetScannerActive(bool active) {
  scanner_active_ = active;
  if (active) {
    StartScan();
  }
}

bool WifiModule::Connect(const char* ssid, const char* password) {
  if (ssid == nullptr || ssid[0] == '\0') return false;
  if (telemetry_.scanning) {
    WiFi.scanDelete();
    telemetry_.scanning = false;
  }

  WiFi.disconnect(false, false);
  delay(30);
  WiFi.mode(WIFI_STA);

  if (password == nullptr || password[0] == '\0') {
    WiFi.begin(ssid);
  } else {
    WiFi.begin(ssid, password);
  }

  strncpy(telemetry_.connected_ssid, ssid, sizeof(telemetry_.connected_ssid) - 1);
  telemetry_.connected_ssid[sizeof(telemetry_.connected_ssid) - 1] = '\0';

  strncpy(pending_ssid_, ssid, sizeof(pending_ssid_) - 1);
  pending_ssid_[sizeof(pending_ssid_) - 1] = 0;
  strncpy(pending_pass_, password ? password : "", sizeof(pending_pass_) - 1);
  pending_pass_[sizeof(pending_pass_) - 1] = 0;
  pending_save_ = true;

  telemetry_.connect_state = WifiConnectState::Connecting;
  telemetry_.connected = false;
  connect_started_ms_ = millis();
  return true;
}

void WifiModule::Disconnect() {
  WiFi.disconnect(true, false);  // leave NVS saved network intact
  telemetry_.connected = false;
  telemetry_.connect_state = WifiConnectState::Idle;
  telemetry_.connected_ssid[0] = '\0';
  telemetry_.link_rssi = -127;
  pending_save_ = false;
}

void WifiModule::CollectResults(int16_t count) {
  network_count_ = 0;
  telemetry_.strongest_rssi = -127;

  if (count <= 0) {
    telemetry_.networks_found = 0;
    WiFi.scanDelete();
    return;
  }

  const uint8_t limit = count > kMaxNetworks ? kMaxNetworks : static_cast<uint8_t>(count);
  for (uint8_t i = 0; i < limit; ++i) {
    WifiNetwork& n = networks_[network_count_++];
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      strncpy(n.ssid, "<hidden>", sizeof(n.ssid) - 1);
    } else {
      strncpy(n.ssid, ssid.c_str(), sizeof(n.ssid) - 1);
    }
    n.ssid[sizeof(n.ssid) - 1] = '\0';
    n.rssi = WiFi.RSSI(i);
    n.channel = static_cast<uint8_t>(WiFi.channel(i));
    n.encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    if (n.rssi > telemetry_.strongest_rssi) {
      telemetry_.strongest_rssi = n.rssi;
    }
  }

  telemetry_.networks_found = static_cast<int16_t>(network_count_);
  WiFi.scanDelete();
}

void WifiModule::Tick() {
  const bool linked = WiFi.status() == WL_CONNECTED;
  telemetry_.connected = linked;
  telemetry_.link_rssi = linked ? WiFi.RSSI() : -127;
  telemetry_.has_saved = saved_ssid_[0] != '\0';
  telemetry_.auto_connect = auto_connect_;

  if (linked && telemetry_.connected_ssid[0] == '\0') {
    String cur = WiFi.SSID();
    strncpy(telemetry_.connected_ssid, cur.c_str(), sizeof(telemetry_.connected_ssid) - 1);
    telemetry_.connected_ssid[sizeof(telemetry_.connected_ssid) - 1] = 0;
  }

  if (telemetry_.connect_state == WifiConnectState::Connecting) {
    if (linked) {
      telemetry_.connect_state = WifiConnectState::Connected;
      if (pending_save_) {
        SaveCredentials(pending_ssid_, pending_pass_);
        pending_save_ = false;
      }
    } else if (millis() - connect_started_ms_ > 15000U) {
      telemetry_.connect_state = WifiConnectState::Failed;
      WiFi.disconnect(false, false);
      pending_save_ = false;
    }
  } else if (telemetry_.connect_state == WifiConnectState::Connected && !linked) {
    telemetry_.connect_state = WifiConnectState::Idle;
    // ESP auto-reconnect may recover; if not and we have saved — retry later
  } else if (telemetry_.connect_state == WifiConnectState::Idle && !linked &&
             auto_connect_ && saved_ssid_[0] && !scanner_active_ && !telemetry_.scanning) {
    static uint32_t last_retry_ms = 0;
    const uint32_t now = millis();
    if (now - last_retry_ms > 30000U) {
      last_retry_ms = now;
      TryAutoConnect();
    }
  }

  if (telemetry_.scanning) {
    const int16_t done = WiFi.scanComplete();
    if (done == WIFI_SCAN_RUNNING) {
      return;
    }
    telemetry_.scanning = false;
    if (done == WIFI_SCAN_FAILED || done < 0) {
      telemetry_.networks_found = 0;
      network_count_ = 0;
      return;
    }
    CollectResults(done);
    return;
  }

  if (scanner_active_ || telemetry_.connected ||
      telemetry_.connect_state == WifiConnectState::Connecting) {
    return;
  }

  const uint32_t now = millis();
  if (now - last_auto_scan_ms_ < 20000U) return;
  last_auto_scan_ms_ = now;
  StartScan();
}

}  // namespace axiom::modules
