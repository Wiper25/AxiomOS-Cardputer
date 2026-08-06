#include "core/app.h"

#include <Arduino.h>
#include <M5Cardputer.h>

#include "core/config.h"

namespace axiom {

bool App::Begin() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  storage_.Begin();
  settings_service_.Begin();
  settings_service_.Load(settings_);

  if (!display_.Begin()) {
    return false;
  }
  if (!keyboard_.Begin()) {
    return false;
  }
  if (!audio_.Begin()) {
    return false;
  }
  audio_.SetVolume(settings_.volume);
  M5.Display.setBrightness(settings_.brightness);
  if (!ui_manager_.Begin(display_)) {
    return false;
  }
  ui_manager_.SetAudioDriver(&audio_);
  ui_manager_.SetWifiModule(&wifi_);
  ui_manager_.SetMqttModule(&mqtt_);
  ui_manager_.SetWebsocketModule(&websocket_);
  ui_manager_.SetHttpModule(&http_);
  ui_manager_.SetTcpModule(&tcp_);
  ui_manager_.SetPingModule(&ping_);
  ui_manager_.SetGpioModule(&gpio_);
  ui_manager_.SetI2cModule(&i2c_);
  ui_manager_.SetSensorsModule(&sensors_);
  ui_manager_.SetStorage(&storage_);
  ui_manager_.SetNrfModule(&nrf24_);
  ui_manager_.SetKeyboard(&keyboard_);
  ui_manager_.SetSettingsSnapshot(settings_);
  nrf24_.Begin();
  nrf24_.SetChannel(settings_.rf_channel);
  nrf24_.SetPower(settings_.rf_power);
  wifi_.Begin();
  mqtt_.Begin();
  websocket_.Begin();
  http_.Begin();
  tcp_.Begin();
  ping_.Begin();
  gpio_.Begin();
  i2c_.Begin();
  sensors_.Begin();
  bt_.Begin();
  storage_.Begin();
  ui::SystemStatus boot_status;
  boot_status.nrf = nrf24_.GetTelemetry();
  boot_status.wifi = wifi_.GetTelemetry();
  boot_status.bt = bt_.GetTelemetry();
  ui_manager_.SetSystemStatus(boot_status);
  audio_.Play(drivers::SoundId::BootMelody);

  const BaseType_t result = xTaskCreatePinnedToCore(
      UiTaskEntry, "axiom_ui", kUiTaskStackWords, this, kUiTaskPriority,
      &ui_task_handle_, 1);
  const BaseType_t nrf_result = xTaskCreatePinnedToCore(
      NrfTaskEntry, "axiom_nrf", kNrfTaskStackWords, this, kNrfTaskPriority,
      &nrf_task_handle_, 0);
  const BaseType_t svc_result = xTaskCreatePinnedToCore(
      ServicesTaskEntry, "axiom_services", kServicesTaskStackWords, this,
      kServicesTaskPriority, &services_task_handle_, 0);

  return result == pdPASS && nrf_result == pdPASS && svc_result == pdPASS;
}

void App::Loop() {
  M5Cardputer.update();

  drivers::InputAction action = drivers::InputAction::None;
  if (keyboard_.Poll(action)) {
    const char ch =
        (action == drivers::InputAction::Char) ? keyboard_.LastChar() : static_cast<char>(0);
    ui_manager_.PostAction(action, ch);
  }
  services::AppSettings updated;
  if (ui_manager_.ConsumeSettingsUpdate(updated)) {
    settings_ = updated;
    M5.Display.setBrightness(settings_.brightness);
    audio_.SetVolume(settings_.volume);
    nrf24_.SetChannel(settings_.rf_channel);
    nrf24_.SetPower(settings_.rf_power);
    settings_service_.Save(settings_);
  }
  audio_.Tick();

  vTaskDelay(pdMS_TO_TICKS(1));
}

void App::UiTaskEntry(void* arg) {
  auto* self = static_cast<App*>(arg);
  self->UiTask();
}

void App::NrfTaskEntry(void* arg) {
  auto* self = static_cast<App*>(arg);
  self->NrfTask();
}

void App::ServicesTaskEntry(void* arg) {
  auto* self = static_cast<App*>(arg);
  self->ServicesTask();
}

void App::UiTask() {
  for (;;) {
    ui_manager_.Tick();
    vTaskDelay(pdMS_TO_TICKS(kUiTaskPeriodMs));
  }
}

void App::NrfTask() {
  for (;;) {
    nrf24_.Tick();
    ui::SystemStatus status;
    status.nrf = nrf24_.GetTelemetry();
    status.wifi = wifi_.GetTelemetry();
    status.bt = bt_.GetTelemetry();
    ui_manager_.SetSystemStatus(status);
    vTaskDelay(pdMS_TO_TICKS(kNrfTaskPeriodMs));
  }
}

void App::ServicesTask() {
  uint32_t last_save_ms = 0;
  uint8_t last_rf_channel = settings_.rf_channel;
  uint8_t last_rf_power = settings_.rf_power;
  for (;;) {
    wifi_.Tick();
    mqtt_.Tick();
    websocket_.Tick();
    http_.Tick();
    tcp_.Tick();
    ping_.Tick();
    gpio_.Tick();
    i2c_.Tick();
    sensors_.Tick();
    storage_.Tick();
    bt_.Tick();

    ui::SystemStatus status;
    status.nrf = nrf24_.GetTelemetry();
    status.wifi = wifi_.GetTelemetry();
    status.bt = bt_.GetTelemetry();
    ui_manager_.SetSystemStatus(status);

    const auto t = status.nrf;
    settings_.rf_channel = t.current_channel;
    settings_.rf_power = t.pa_level;
    const bool changed =
        (settings_.rf_channel != last_rf_channel) || (settings_.rf_power != last_rf_power);
    const uint32_t now = millis();
    if (changed && (now - last_save_ms >= 30000U)) {
      settings_service_.Save(settings_);
      last_save_ms = now;
      last_rf_channel = settings_.rf_channel;
      last_rf_power = settings_.rf_power;
    }

    vTaskDelay(pdMS_TO_TICKS(kServicesTaskPeriodMs));
  }
}

}  // namespace axiom
