#include "modules/nrf24/nrf24_module.h"

#include <Arduino.h>
#include <string.h>

#include "core/config.h"

namespace axiom::modules {

Nrf24Module::Nrf24Module()
    : spi_bus_(FSPI),
      radio_(axiom::kNrfCePin, axiom::kNrfCsnPin) {}

bool Nrf24Module::Begin() {
  spi_bus_.begin(axiom::kNrfSckPin, axiom::kNrfMisoPin, axiom::kNrfMosiPin,
                 axiom::kNrfCsnPin);
  if (!radio_.begin(&spi_bus_, axiom::kNrfCePin, axiom::kNrfCsnPin)) {
    telemetry_.present = false;
    return false;
  }
  telemetry_.present = radio_.isChipConnected();
  if (!telemetry_.present) return false;

  radio_.setAutoAck(false);
  radio_.disableCRC();
  radio_.setRetries(1, 3);
  radio_.setDataRate(RF24_1MBPS);
  data_rate_ = 0;
  radio_.setPALevel(RF24_PA_HIGH, true);
  telemetry_.pa_level = RF24_PA_HIGH;
  radio_.setChannel(76);
  telemetry_.current_channel = 76;
  telemetry_.hottest_channel = 76;
  ApplyListenConfig();
  return true;
}

void Nrf24Module::ApplyListenConfig() {
  static const uint8_t addr[6] = "AXIOM";
  radio_.openReadingPipe(1, addr);
  radio_.startListening();
}

bool Nrf24Module::SetPower(uint8_t pa_level) {
  if (!telemetry_.present || pa_level > 3) return false;
  radio_.setPALevel(pa_level, true);
  telemetry_.pa_level = pa_level;
  return true;
}

bool Nrf24Module::SetChannel(uint8_t channel) {
  if (!telemetry_.present || channel > 125) return false;
  radio_.setChannel(channel);
  telemetry_.current_channel = channel;
  return true;
}

bool Nrf24Module::SetDataRate(uint8_t rate) {
  if (!telemetry_.present || rate > 2) return false;
  rf24_datarate_e dr = RF24_1MBPS;
  if (rate == 1) dr = RF24_2MBPS;
  else if (rate == 2) dr = RF24_250KBPS;
  if (!radio_.setDataRate(dr)) return false;
  data_rate_ = rate;
  return true;
}

void Nrf24Module::SetScannerActive(bool active) {
  scanner_active_ = active;
  telemetry_.scanning = active;
  if (active) {
    monitor_active_ = false;
    telemetry_.monitoring = false;
    scan_ch_ = 0;
    radio_.stopListening();
  } else if (!monitor_active_) {
    ApplyListenConfig();
  }
}

void Nrf24Module::SetMonitorActive(bool active) {
  monitor_active_ = active;
  telemetry_.monitoring = active;
  if (active) {
    scanner_active_ = false;
    telemetry_.scanning = false;
    ApplyListenConfig();
  } else if (!scanner_active_) {
    ApplyListenConfig();
  }
}

uint8_t Nrf24Module::ActivityAt(uint8_t ch) const {
  return ch < kMaxChannels ? activity_map_[ch] : 0;
}

void Nrf24Module::ClearPackets() {
  packet_count_ = 0;
  telemetry_.packets_rx = 0;
}

void Nrf24Module::ScanStep() {
  constexpr uint8_t kPerTick = 6;
  constexpr uint8_t kSamples = 8;

  uint8_t best_local = 0;
  uint8_t best_ch = telemetry_.hottest_channel;

  for (uint8_t n = 0; n < kPerTick && scan_ch_ < kMaxChannels; ++n, ++scan_ch_) {
    uint16_t hits = 0;
    radio_.setChannel(scan_ch_);
    for (uint8_t i = 0; i < kSamples; ++i) {
      if (radio_.testRPD()) ++hits;
      delayMicroseconds(120);
    }
    const uint8_t pct = static_cast<uint8_t>((hits * 100U) / kSamples);
    activity_map_[scan_ch_] = pct;
    if (pct >= best_local) {
      best_local = pct;
      best_ch = scan_ch_;
    }
  }

  if (best_local >= telemetry_.activity_percent || scan_ch_ >= kMaxChannels) {
    if (best_local > 0) {
      telemetry_.activity_percent = best_local;
      telemetry_.hottest_channel = best_ch;
    }
  }

  // recompute hottest across map occasionally at wrap
  if (scan_ch_ >= kMaxChannels) {
    uint8_t hi = 0;
    uint8_t hi_ch = telemetry_.current_channel;
    for (uint8_t ch = 0; ch < kMaxChannels; ++ch) {
      if (activity_map_[ch] >= hi) {
        hi = activity_map_[ch];
        hi_ch = ch;
      }
    }
    telemetry_.activity_percent = hi;
    telemetry_.hottest_channel = hi_ch;
    scan_ch_ = 0;
    ++telemetry_.scan_counter;
  }
}

void Nrf24Module::PollRx() {
  while (radio_.available() && packet_count_ < kMaxPackets) {
    NrfPacket& p = packets_[packet_count_];
    p.len = radio_.getPayloadSize();
    if (p.len > 32) p.len = 32;
    radio_.read(p.data, p.len);
    p.channel = telemetry_.current_channel;
    p.ms = millis();
    ++packet_count_;
    ++telemetry_.packets_rx;
  }
  // drop oldest if full and more coming
  if (packet_count_ >= kMaxPackets && radio_.available()) {
    memmove(&packets_[0], &packets_[1], sizeof(NrfPacket) * (kMaxPackets - 1));
    --packet_count_;
    NrfPacket& p = packets_[packet_count_];
    p.len = radio_.getPayloadSize();
    if (p.len > 32) p.len = 32;
    radio_.read(p.data, p.len);
    p.channel = telemetry_.current_channel;
    p.ms = millis();
    ++packet_count_;
    ++telemetry_.packets_rx;
  }
}

bool Nrf24Module::TxTestPacket() {
  if (!telemetry_.present) return false;
  static const uint8_t addr[6] = "AXIOM";
  const uint32_t payload = millis();

  const bool was_mon = monitor_active_;
  const bool was_scan = scanner_active_;
  radio_.stopListening();
  radio_.setAutoAck(true);
  radio_.setRetries(5, 15);
  radio_.openWritingPipe(addr);
  const bool ok = radio_.write(&payload, sizeof(payload), false);
  radio_.setAutoAck(false);
  telemetry_.tx_ok = ok;

  if (was_scan) {
    radio_.stopListening();
  } else {
    ApplyListenConfig();
    monitor_active_ = was_mon;
    telemetry_.monitoring = was_mon;
  }
  return ok;
}

void Nrf24Module::Tick() {
  if (!telemetry_.present) return;

  if (monitor_active_) {
    PollRx();
    return;
  }

  if (scanner_active_) {
    ScanStep();
    return;
  }

  // light background hop — one channel sample, non-blocking-ish
  const uint32_t now = millis();
  if (now - last_bg_scan_ms_ < 80) return;
  last_bg_scan_ms_ = now;

  uint16_t hits = 0;
  radio_.stopListening();
  radio_.setChannel(scan_ch_);
  for (uint8_t i = 0; i < 4; ++i) {
    if (radio_.testRPD()) ++hits;
    delayMicroseconds(100);
  }
  activity_map_[scan_ch_] = static_cast<uint8_t>((hits * 100U) / 4);
  if (activity_map_[scan_ch_] >= telemetry_.activity_percent) {
    telemetry_.activity_percent = activity_map_[scan_ch_];
    telemetry_.hottest_channel = scan_ch_;
  }
  scan_ch_ = static_cast<uint8_t>((scan_ch_ + 1) % kMaxChannels);
  if (scan_ch_ == 0) ++telemetry_.scan_counter;
  radio_.setChannel(telemetry_.current_channel);
  ApplyListenConfig();
}

}  // namespace axiom::modules
