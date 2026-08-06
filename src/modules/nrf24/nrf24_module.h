#pragma once

#include <RF24.h>
#include <SPI.h>
#include <stdint.h>

namespace axiom::modules {

struct NrfTelemetry {
  bool present = false;
  uint8_t current_channel = 76;
  uint8_t activity_percent = 0;
  uint8_t pa_level = RF24_PA_LOW;
  uint32_t scan_counter = 0;
  bool tx_ok = false;
  bool scanning = false;
  bool monitoring = false;
  uint16_t packets_rx = 0;
  uint8_t hottest_channel = 76;
};

struct NrfPacket {
  uint8_t len = 0;
  uint8_t channel = 0;
  uint32_t ms = 0;
  uint8_t data[32] = {0};
};

class Nrf24Module {
 public:
  static constexpr uint8_t kMaxChannels = 126;
  static constexpr uint8_t kMaxPackets = 12;

  Nrf24Module();
  bool Begin();
  bool IsPresent() const { return telemetry_.present; }
  bool SetPower(uint8_t pa_level);
  bool SetChannel(uint8_t channel);
  bool SetDataRate(uint8_t rate);  // 0=1Mbps 1=2Mbps 2=250kbps
  uint8_t DataRate() const { return data_rate_; }

  void SetScannerActive(bool active);
  void SetMonitorActive(bool active);
  bool TxTestPacket();
  void Tick();

  uint8_t ActivityAt(uint8_t ch) const;
  uint8_t PacketCount() const { return packet_count_; }
  const NrfPacket& PacketAt(uint8_t i) const { return packets_[i]; }
  void ClearPackets();
  NrfTelemetry GetTelemetry() const { return telemetry_; }

 private:
  void ScanStep();
  void PollRx();
  void ApplyListenConfig();

  SPIClass spi_bus_;
  RF24 radio_;
  NrfTelemetry telemetry_;
  uint8_t activity_map_[kMaxChannels] = {0};
  uint8_t scan_ch_ = 0;
  uint8_t data_rate_ = 0;
  bool scanner_active_ = false;
  bool monitor_active_ = false;
  NrfPacket packets_[kMaxPackets];
  uint8_t packet_count_ = 0;
  uint32_t last_bg_scan_ms_ = 0;
};

}  // namespace axiom::modules
