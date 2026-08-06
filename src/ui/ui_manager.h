#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "drivers/audio/audio_driver.h"
#include "drivers/keyboard/keyboard_driver.h"
#include "drivers/display/display_driver.h"
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

namespace axiom::ui {

struct SystemStatus {
  modules::WifiTelemetry wifi;
  modules::BluetoothTelemetry bt;
  modules::NrfTelemetry nrf;
};

struct UiInputEvent {
  drivers::InputAction action = drivers::InputAction::None;
  char ch = 0;
};

enum class NetToolId : uint8_t {
  None = 0,
  Websocket,
  Http,
  Tcp,
  Ping,
  Info
};

enum class HwToolId : uint8_t {
  None = 0,
  Gpio,
  I2c,
  Sensors
};

enum class RadioToolId : uint8_t {
  None = 0,
  Scanner,
  Monitor,
  Manager
};

class UiManager {
 public:
  bool Begin(drivers::DisplayDriver& display);
  void SetAudioDriver(drivers::AudioDriver* audio) { audio_ = audio; }
  void SetWifiModule(modules::WifiModule* wifi) { wifi_ = wifi; }
  void SetMqttModule(modules::MqttModule* mqtt) { mqtt_ = mqtt; }
  void SetWebsocketModule(modules::WebsocketModule* ws) { websocket_ = ws; }
  void SetHttpModule(modules::HttpModule* http) { http_ = http; }
  void SetTcpModule(modules::TcpModule* tcp) { tcp_ = tcp; }
  void SetPingModule(modules::PingModule* ping) { ping_ = ping; }
  void SetGpioModule(modules::GpioModule* gpio) { gpio_ = gpio; }
  void SetI2cModule(modules::I2cModule* i2c) { i2c_ = i2c; }
  void SetSensorsModule(modules::SensorsModule* sensors) { sensors_ = sensors; }
  void SetStorage(services::StorageService* storage) { storage_ = storage; }
  void SetNrfModule(modules::Nrf24Module* nrf) { nrf_ = nrf; }
  void SetKeyboard(drivers::KeyboardDriver* kb) { keyboard_ = kb; }
  void SetNrfTelemetry(const modules::NrfTelemetry& telemetry);
  void SetSystemStatus(const SystemStatus& status);
  void SetSettingsSnapshot(const services::AppSettings& s) { settings_ = s; }
  bool ConsumeSettingsUpdate(services::AppSettings& out);
  void Tick();
  void PostAction(drivers::InputAction action, char ch = 0);

 private:
  void BuildTheme();
  void BuildBootScreen();
  void AnimateBootLogo();
  void BuildMenuScreen(bool forward);
  void RefreshMenuList(bool animate_in);
  void RefreshStatusBar();
  void RefreshWifiScannerList(bool animate_in);
  void RefreshWifiPasswordScreen();
  void RefreshMqttScreen(bool animate_in);
  void RefreshNetToolScreen(bool animate_in);
  void RefreshHwToolScreen(bool animate_in);
  void RefreshRadioScreen(bool animate_in);
  void RefreshFsBrowser(bool animate_in);
  void RefreshAboutScreen(bool animate_in);
  void HandleInput(const UiInputEvent& event);
  void HandleSettingsInput(drivers::InputAction action);
  void HandleWifiScannerInput(const UiInputEvent& event);
  void HandleWifiPasswordInput(const UiInputEvent& event);
  void HandleMqttInput(const UiInputEvent& event);
  void HandleNetToolInput(const UiInputEvent& event);
  void HandleHwToolInput(const UiInputEvent& event);
  void HandleRadioInput(const UiInputEvent& event);
  void HandleFsBrowserInput(const UiInputEvent& event);
  void HandleAboutInput(drivers::InputAction action);
  void SelectCurrentItem();
  void OpenWifiScanner();
  void CloseWifiScanner();
  void OpenWifiPassword();
  void CloseWifiPassword(bool back_to_list);
  void BeginWifiConnect();
  void OpenMqttClient();
  void CloseMqttClient();
  void BeginMqttEdit();
  void CommitMqttEdit();
  void CancelMqttEdit();
  void OpenNetTool(NetToolId id);
  void CloseNetTool();
  void BeginNetEdit();
  void CommitNetEdit();
  void CancelNetEdit();
  void RunNetAction();
  int NetToolRowCount() const;
  void FillNetToolLines(char lines[][40], int count);
  void OpenHwTool(HwToolId id);
  void CloseHwTool();
  void OpenRadioTool(RadioToolId id);
  void CloseRadioTool();
  void EnsureRadioSpecUi();
  void OpenSensorGraph(bool gyro);
  void CloseSensorGraph();
  void EnsureSensorGraphUi();
  void RefreshSensorGraph();
  void OpenFsBrowser();
  void CloseFsBrowser();
  void OpenAbout();
  void CloseAbout();
  void FormatBytes(char* dst, size_t n, uint64_t bytes);
  void StyleModeIcon(lv_obj_t* icon, bool active, bool warn = false);
  void TintStatusIcon(lv_obj_t* icon, uint32_t color);
  lv_obj_t* CreateWifiIcon(lv_obj_t* parent);
  lv_obj_t* CreateRfIcon(lv_obj_t* parent);
  lv_obj_t* CreateBtIcon(lv_obj_t* parent);
  void AnimateSelectionCursor(int index);
  void AnimateItemsIn(int count, bool forward);
  void LayoutListRows(int32_t step);
  void TruncateText(char* dst, size_t dst_size, const char* src, size_t max_chars);
  static const char* SignalBars(int32_t rssi);
  static int32_t MapSensorBar(float v, float limit);

  lv_obj_t* boot_screen_ = nullptr;
  lv_obj_t* menu_screen_ = nullptr;
  lv_obj_t* logo_label_ = nullptr;
  lv_obj_t* loading_bar_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
  lv_obj_t* icon_wifi_ = nullptr;
  lv_obj_t* icon_rf_ = nullptr;
  lv_obj_t* icon_bt_ = nullptr;
  lv_obj_t* icon_bat_ = nullptr;
  lv_obj_t* breadcrumb_label_ = nullptr;
  lv_obj_t* title_line_ = nullptr;
  lv_obj_t* hint_label_ = nullptr;
  lv_obj_t* menu_container_ = nullptr;
  lv_obj_t* selection_cursor_ = nullptr;
  lv_obj_t* menu_rows_[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* menu_texts_[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

  lv_style_t style_screen_;
  lv_style_t style_logo_;
  lv_style_t style_bar_bg_;
  lv_style_t style_bar_indic_;
  lv_style_t style_status_;
  lv_style_t style_item_;
  lv_style_t style_item_selected_;
  lv_style_t style_cursor_;

  QueueHandle_t input_queue_ = nullptr;
  bool in_submenu_ = false;
  int selected_index_ = 0;
  int menu_scroll_ = 0;
  int active_section_ = 0;
  drivers::AudioDriver* audio_ = nullptr;
  drivers::KeyboardDriver* keyboard_ = nullptr;
  modules::WifiModule* wifi_ = nullptr;
  modules::MqttModule* mqtt_ = nullptr;
  modules::WebsocketModule* websocket_ = nullptr;
  modules::HttpModule* http_ = nullptr;
  modules::TcpModule* tcp_ = nullptr;
  modules::PingModule* ping_ = nullptr;
  modules::GpioModule* gpio_ = nullptr;
  modules::I2cModule* i2c_ = nullptr;
  modules::SensorsModule* sensors_ = nullptr;
  modules::Nrf24Module* nrf_ = nullptr;
  services::StorageService* storage_ = nullptr;
  modules::NrfTelemetry nrf_telemetry_;
  SystemStatus system_status_;
  services::AppSettings settings_;
  bool settings_dirty_ = false;
  bool in_settings_screen_ = false;
  bool settings_edit_mode_ = false;
  int settings_index_ = 0;
  int settings_scroll_ = 0;
  bool in_wifi_scanner_ = false;
  bool in_wifi_password_ = false;
  int wifi_selected_ = 0;
  int wifi_scroll_ = 0;
  uint8_t last_wifi_count_ = 0;
  bool last_wifi_scanning_ = false;
  modules::WifiConnectState last_connect_state_ = modules::WifiConnectState::Idle;
  char wifi_target_ssid_[33] = {0};
  bool wifi_target_encrypted_ = false;
  char wifi_password_[65] = {0};
  uint8_t wifi_password_len_ = 0;
  bool in_mqtt_screen_ = false;
  bool mqtt_edit_mode_ = false;
  int mqtt_selected_ = 0;
  int mqtt_scroll_ = 0;
  modules::MqttState last_mqtt_state_ = modules::MqttState::Idle;
  bool last_mqtt_has_rx_ = false;
  char mqtt_edit_buf_[64] = {0};
  uint8_t mqtt_edit_len_ = 0;
  NetToolId net_tool_ = NetToolId::None;
  bool net_edit_mode_ = false;
  int net_selected_ = 0;
  int net_scroll_ = 0;
  char net_edit_buf_[96] = {0};
  uint8_t net_edit_len_ = 0;
  char ntp_status_[24] = "—";
  HwToolId hw_tool_ = HwToolId::None;
  int hw_selected_ = 0;
  int hw_scroll_ = 0;
  RadioToolId radio_tool_ = RadioToolId::None;
  int radio_selected_ = 0;
  int radio_scroll_ = 0;
  int radio_spec_origin_ = 0;
  lv_obj_t* radio_spec_panel_ = nullptr;
  lv_obj_t* radio_spec_bars_[32] = {nullptr};
  lv_obj_t* radio_spec_label_ = nullptr;
  bool in_sensor_graph_ = false;
  bool sensor_graph_gyro_ = false;
  lv_obj_t* graph_panel_ = nullptr;
  lv_obj_t* graph_pad_ = nullptr;
  lv_obj_t* graph_cross_h_ = nullptr;
  lv_obj_t* graph_cross_v_ = nullptr;
  lv_obj_t* graph_ring_ = nullptr;
  lv_obj_t* graph_bubble_ = nullptr;
  lv_obj_t* graph_bars_[3] = {nullptr, nullptr, nullptr};
  lv_obj_t* graph_bar_labels_[3] = {nullptr, nullptr, nullptr};
  lv_obj_t* graph_value_label_ = nullptr;
  float graph_sx_ = 0, graph_sy_ = 0, graph_sz_ = 0;
  float graph_bx_ = 0, graph_by_ = 0;
  bool last_i2c_scanning_ = false;
  uint8_t last_i2c_count_ = 0;
  uint32_t last_hw_list_ms_ = 0;
  bool in_fs_browser_ = false;
  int fs_selected_ = 0;
  int fs_scroll_ = 0;
  bool in_about_screen_ = false;
  int about_selected_ = 0;
};

}  // namespace axiom::ui
