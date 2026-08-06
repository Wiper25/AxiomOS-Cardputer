#include "modules/wifi/wifi_module.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

namespace axiom::modules {

bool WifiModule::Begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);
  telemetry_.station_mode = true;
  telemetry_.connected = false;
  telemetry_.scanning = false;
  telemetry_.connect_state = WifiConnectState::Idle;
  telemetry_.connected_ssid[0] = '\0';
  network_count_ = 0;
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
  telemetry_.connect_state = WifiConnectState::Connecting;
  telemetry_.connected = false;
  connect_started_ms_ = millis();
  return true;
}

void WifiModule::Disconnect() {
  WiFi.disconnect(true, false);
  telemetry_.connected = false;
  telemetry_.connect_state = WifiConnectState::Idle;
  telemetry_.connected_ssid[0] = '\0';
  telemetry_.link_rssi = -127;
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

  if (telemetry_.connect_state == WifiConnectState::Connecting) {
    if (linked) {
      telemetry_.connect_state = WifiConnectState::Connected;
    } else if (millis() - connect_started_ms_ > 15000U) {
      telemetry_.connect_state = WifiConnectState::Failed;
      WiFi.disconnect(false, false);
    }
  } else if (telemetry_.connect_state == WifiConnectState::Connected && !linked) {
    telemetry_.connect_state = WifiConnectState::Idle;
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
