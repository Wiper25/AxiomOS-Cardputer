#include "modules/mqtt/mqtt_module.h"

#include <Arduino.h>
#include <string.h>

namespace axiom::modules {

MqttModule* MqttModule::instance_ = nullptr;

MqttModule::MqttModule() : mqtt_(wifi_client_) {}

bool MqttModule::Begin() {
  instance_ = this;
  mqtt_.setBufferSize(256);
  mqtt_.setCallback(OnMessageThunk);
  telemetry_.state = MqttState::Idle;
  return true;
}

void MqttModule::OnMessageThunk(char* topic, uint8_t* payload, unsigned int length) {
  if (instance_ != nullptr) {
    instance_->OnMessage(topic, payload, length);
  }
}

void MqttModule::OnMessage(char* topic, uint8_t* payload, unsigned int length) {
  strncpy(telemetry_.last_rx_topic, topic != nullptr ? topic : "", sizeof(telemetry_.last_rx_topic) - 1);
  telemetry_.last_rx_topic[sizeof(telemetry_.last_rx_topic) - 1] = '\0';

  const unsigned int n =
      length < sizeof(telemetry_.last_rx_payload) - 1 ? length : sizeof(telemetry_.last_rx_payload) - 1;
  memcpy(telemetry_.last_rx_payload, payload, n);
  telemetry_.last_rx_payload[n] = '\0';
  telemetry_.has_rx = true;
}

bool MqttModule::Connect() {
  if (WiFi.status() != WL_CONNECTED) {
    telemetry_.state = MqttState::Error;
    return false;
  }
  if (mqtt_.connected()) {
    telemetry_.state = MqttState::Connected;
    return true;
  }

  mqtt_.setServer(config_.host, config_.port);
  telemetry_.state = MqttState::Connecting;

  const bool ok = mqtt_.connect(config_.client_id);
  if (!ok) {
    telemetry_.state = MqttState::Error;
    return false;
  }

  if (config_.sub_topic[0] != '\0') {
    mqtt_.subscribe(config_.sub_topic);
  }
  telemetry_.state = MqttState::Connected;
  return true;
}

void MqttModule::Disconnect() {
  if (mqtt_.connected()) {
    mqtt_.disconnect();
  }
  telemetry_.state = MqttState::Idle;
}

bool MqttModule::Publish() {
  if (!mqtt_.connected()) {
    if (!Connect()) return false;
  }
  return mqtt_.publish(config_.pub_topic, config_.message);
}

void MqttModule::Tick() {
  if (mqtt_.connected()) {
    mqtt_.loop();
    telemetry_.state = MqttState::Connected;
    return;
  }

  if (telemetry_.state == MqttState::Connected) {
    telemetry_.state = MqttState::Idle;
  }
}

}  // namespace axiom::modules
