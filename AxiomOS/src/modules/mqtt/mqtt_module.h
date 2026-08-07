#pragma once

#include <PubSubClient.h>
#include <WiFi.h>
#include <stdint.h>

namespace axiom::modules {

enum class MqttState : uint8_t {
  Idle = 0,
  Connecting,
  Connected,
  Error
};

struct MqttConfig {
  char host[64] = "broker.hivemq.com";
  uint16_t port = 1883;
  char client_id[24] = "axiom-cardputer";
  char sub_topic[48] = "axiom/cardputer/#";
  char pub_topic[48] = "axiom/cardputer/out";
  char message[48] = "hello";
};

struct MqttTelemetry {
  MqttState state = MqttState::Idle;
  bool has_rx = false;
  char last_rx_topic[48] = {0};
  char last_rx_payload[64] = {0};
};

class MqttModule {
 public:
  MqttModule();
  bool Begin();
  void Tick();
  bool Connect();
  void Disconnect();
  bool Publish();
  bool IsConnected() const { return telemetry_.state == MqttState::Connected; }
  MqttConfig& Config() { return config_; }
  const MqttConfig& Config() const { return config_; }
  MqttTelemetry GetTelemetry() const { return telemetry_; }

 private:
  static void OnMessageThunk(char* topic, uint8_t* payload, unsigned int length);
  void OnMessage(char* topic, uint8_t* payload, unsigned int length);

  WiFiClient wifi_client_;
  PubSubClient mqtt_;
  MqttConfig config_;
  MqttTelemetry telemetry_;
  static MqttModule* instance_;
};

}  // namespace axiom::modules
