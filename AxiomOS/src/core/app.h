#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "drivers/display/display_driver.h"
#include "drivers/keyboard/keyboard_driver.h"
#include "drivers/audio/audio_driver.h"
#include "modules/bluetooth/bluetooth_module.h"
#include "modules/gpio/gpio_module.h"
#include "modules/http/http_module.h"
#include "modules/i2c/i2c_module.h"
#include "modules/mqtt/mqtt_module.h"
#include "modules/nrf24/nrf24_module.h"
#include "modules/ping/ping_module.h"
#include "modules/sensors/sensors_module.h"
#include "modules/tcp/tcp_module.h"
#include "modules/websocket/websocket_module.h"
#include "modules/wifi/wifi_module.h"
#include "services/settings/settings_service.h"
#include "services/storage/storage_service.h"
#include "ui/ui_manager.h"

#if AXIOM_AI
#include "modules/ai/AIManager.h"
#include "modules/ai/AIUI.h"
#endif
#if AXIOM_VOICE
#include "modules/voice/voice_assistant.h"
#endif

namespace axiom {

class App {
 public:
  bool Begin();
  void Loop();

 private:
  static void UiTaskEntry(void* arg);
  static void NrfTaskEntry(void* arg);
  static void ServicesTaskEntry(void* arg);
#if AXIOM_AI
  static void AiTaskEntry(void* arg);
  void AiTask();
#endif
  void UiTask();
  void NrfTask();
  void ServicesTask();

  drivers::DisplayDriver display_;
  drivers::KeyboardDriver keyboard_;
  drivers::AudioDriver audio_;
  modules::Nrf24Module nrf24_;
  modules::WifiModule wifi_;
  modules::MqttModule mqtt_;
  modules::WebsocketModule websocket_;
  modules::HttpModule http_;
  modules::TcpModule tcp_;
  modules::PingModule ping_;
  modules::GpioModule gpio_;
  modules::I2cModule i2c_;
  modules::SensorsModule sensors_;
  modules::BluetoothModule bt_;
  services::StorageService storage_;
  services::SettingsService settings_service_;
  services::AppSettings settings_;
  ui::UiManager ui_manager_;
#if AXIOM_AI
  ai::AIManager ai_manager_;
  ai::AIUI ai_ui_;
  TaskHandle_t ai_task_handle_ = nullptr;
#endif
#if AXIOM_VOICE
  voice::VoiceAssistant voice_;
#endif
  TaskHandle_t ui_task_handle_ = nullptr;
  TaskHandle_t nrf_task_handle_ = nullptr;
  TaskHandle_t services_task_handle_ = nullptr;
};

}  // namespace axiom
