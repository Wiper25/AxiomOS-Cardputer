#pragma once

#include <stdint.h>

namespace axiom::modules {

struct WifiNetwork {
  char ssid[33] = {0};
  int32_t rssi = -127;
  uint8_t channel = 0;
  bool encrypted = false;
};

enum class WifiConnectState : uint8_t {
  Idle = 0,
  Connecting,
  Connected,
  Failed
};

struct WifiTelemetry {
  bool station_mode = false;
  bool connected = false;
  bool scanning = false;
  WifiConnectState connect_state = WifiConnectState::Idle;
  int16_t networks_found = 0;
  int32_t strongest_rssi = -127;
  int32_t link_rssi = -127;
  char connected_ssid[33] = {0};
  bool has_saved = false;
  bool auto_connect = true;
};

class WifiModule {
 public:
  static constexpr uint8_t kMaxNetworks = 12;

  bool Begin();
  void Tick();
  bool StartScan();
  void SetScannerActive(bool active);
  bool Connect(const char* ssid, const char* password);
  void Disconnect();
  void ForgetSaved();  // wipe NVS creds
  bool HasSaved() const { return saved_ssid_[0] != '\0'; }
  const char* SavedSsid() const { return saved_ssid_; }
  bool ConnectSaved();  // reconnect using NVS creds
  bool IsScanning() const { return telemetry_.scanning; }
  WifiConnectState ConnectState() const { return telemetry_.connect_state; }
  uint8_t NetworkCount() const { return network_count_; }
  const WifiNetwork& NetworkAt(uint8_t index) const { return networks_[index]; }
  WifiTelemetry GetTelemetry() const { return telemetry_; }

 private:
  void CollectResults(int16_t count);
  bool LoadCredentials();
  bool SaveCredentials(const char* ssid, const char* password);
  void TryAutoConnect();

  WifiTelemetry telemetry_;
  WifiNetwork networks_[kMaxNetworks];
  uint8_t network_count_ = 0;
  uint32_t last_auto_scan_ms_ = 0;
  uint32_t connect_started_ms_ = 0;
  bool scanner_active_ = false;

  char saved_ssid_[33] = {0};
  char saved_pass_[65] = {0};
  char pending_ssid_[33] = {0};
  char pending_pass_[65] = {0};
  bool pending_save_ = false;
  bool auto_connect_ = true;
};

}  // namespace axiom::modules
