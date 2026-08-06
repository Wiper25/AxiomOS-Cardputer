#include "ui/ui_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "ui/fonts/font_ru_14.h"
#include "core/config.h"

namespace axiom::ui {

namespace {
constexpr uint32_t kBgColorHex = 0x070A12;
constexpr uint32_t kPrimaryHex = 0x00D1FF;
constexpr uint32_t kSecondaryHex = 0x7B2CFF;
constexpr uint32_t kMutedHex = 0x1C2738;
constexpr int32_t kRowH = 20;
constexpr int32_t kRowGap = 1;
constexpr int32_t kRowStep = kRowH + kRowGap;

struct MenuNode {
  const char* title;
  const char* items[8];
  int item_count;
};

constexpr MenuNode kRootMenu = {"AxiomOS",
                                {"Радио", "Сеть", "Железо", "Система"},
                                4};

constexpr MenuNode kSubmenus[] = {
    {"Радио", {"Сканер спектра", "Монитор пакетов", "Менеджер nRF24"}, 3},
    {"Сеть",
     {"Сканер WiFi", "MQTT клиент", "Вебсокет", "HTTP клиент", "TCP клиент", "Пинг / DNS",
      "Сеть / IP"},
     7},
    {"Железо", {"Монитор GPIO", "Сканер I2C", "Датчики"}, 3},
    {"Система", {"Настройки", "Файлы / флеш", "О системе", "Обновление"}, 4},
};

constexpr int kVisibleRows = 4;
}  // namespace

static void BootProgressAnimationCb(void* bar, int32_t v) {
  lv_bar_set_value(static_cast<lv_obj_t*>(bar), v, LV_ANIM_OFF);
}

static void BootFadeAnimationCb(void* obj, int32_t v) {
  lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

static void AnimTranslateX(void* obj, int32_t v) {
  lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(obj), v, 0);
}

static void AnimTranslateY(void* obj, int32_t v) {
  lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj), v, 0);
}

static void AnimOpa(void* obj, int32_t v) {
  lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

bool UiManager::Begin(drivers::DisplayDriver& display) {
  (void)display;
  input_queue_ = xQueueCreate(16, sizeof(UiInputEvent));
  if (input_queue_ == nullptr) {
    return false;
  }

  BuildTheme();
  BuildBootScreen();
  BuildMenuScreen(true);
  AnimateBootLogo();
  return true;
}

void UiManager::Tick() {
  UiInputEvent event;
  while (xQueueReceive(input_queue_, &event, 0) == pdTRUE) {
    HandleInput(event);
  }
  RefreshStatusBar();
  if (in_wifi_scanner_ && wifi_ != nullptr && !in_wifi_password_) {
    const bool scanning = wifi_->IsScanning();
    const uint8_t count = wifi_->NetworkCount();
    if (scanning != last_wifi_scanning_ || count != last_wifi_count_) {
      last_wifi_scanning_ = scanning;
      last_wifi_count_ = count;
      RefreshWifiScannerList(false);
    }
  }
  if (in_wifi_password_ && wifi_ != nullptr) {
    const auto st = wifi_->ConnectState();
    if (st != last_connect_state_) {
      last_connect_state_ = st;
      RefreshWifiPasswordScreen();
      if (st == modules::WifiConnectState::Connected && audio_) {
        audio_->Play(drivers::SoundId::Success);
      } else if (st == modules::WifiConnectState::Failed && audio_) {
        audio_->Play(drivers::SoundId::Error);
      }
    }
  }
  if (in_mqtt_screen_ && mqtt_ != nullptr && !mqtt_edit_mode_) {
    const auto tel = mqtt_->GetTelemetry();
    if (tel.state != last_mqtt_state_ || tel.has_rx != last_mqtt_has_rx_) {
      last_mqtt_state_ = tel.state;
      last_mqtt_has_rx_ = tel.has_rx;
      RefreshMqttScreen(false);
    }
  }
  if (net_tool_ == NetToolId::Websocket && websocket_ != nullptr && !net_edit_mode_) {
    static modules::WsState last_ws = modules::WsState::Idle;
    static bool last_ws_rx = false;
    const auto tel = websocket_->GetTelemetry();
    if (tel.state != last_ws || tel.has_rx != last_ws_rx) {
      last_ws = tel.state;
      last_ws_rx = tel.has_rx;
      RefreshNetToolScreen(false);
    }
  } else if (net_tool_ == NetToolId::Tcp && tcp_ != nullptr && !net_edit_mode_) {
    static modules::TcpState last_tcp = modules::TcpState::Idle;
    static bool last_tcp_rx = false;
    const auto tel = tcp_->GetTelemetry();
    if (tel.state != last_tcp || tel.has_rx != last_tcp_rx) {
      last_tcp = tel.state;
      last_tcp_rx = tel.has_rx;
      RefreshNetToolScreen(false);
    }
  } else if (net_tool_ == NetToolId::Http && http_ != nullptr && !net_edit_mode_) {
    static modules::HttpState last_http = modules::HttpState::Idle;
    const auto tel = http_->GetTelemetry();
    if (tel.state != last_http) {
      last_http = tel.state;
      RefreshNetToolScreen(false);
    }
  }
  if (radio_tool_ == RadioToolId::Scanner || radio_tool_ == RadioToolId::Monitor) {
    static uint32_t last_radio_ui_ms = 0;
    const uint32_t now_r = millis();
    if (now_r - last_radio_ui_ms >= 120) {
      last_radio_ui_ms = now_r;
      RefreshRadioScreen(false);
    }
  }
  if (hw_tool_ == HwToolId::I2c && i2c_ != nullptr) {
    i2c_->Tick();
    const bool scanning = i2c_->Scanning();
    const uint8_t count = i2c_->Count();
    static uint32_t last_i2c_ui_ms = 0;
    const uint32_t now = millis();
    const bool done = last_i2c_scanning_ && !scanning;
    if (done || scanning != last_i2c_scanning_ ||
        (scanning && now - last_i2c_ui_ms >= 150) ||
        (!scanning && count != last_i2c_count_)) {
      last_i2c_scanning_ = scanning;
      last_i2c_count_ = count;
      last_i2c_ui_ms = now;
      RefreshHwToolScreen(false);
      if (done && audio_) audio_->Play(drivers::SoundId::Success);
    }
  }
  if (in_sensor_graph_) {
    if (sensors_ != nullptr) {
      sensors_->SetLiveMode(true);
      sensors_->Tick();
    }
    RefreshSensorGraph();
  } else if (hw_tool_ == HwToolId::Sensors || hw_tool_ == HwToolId::Gpio) {
    if (sensors_ != nullptr) sensors_->SetLiveMode(false);
    const uint32_t now = millis();
    if (now - last_hw_list_ms_ >= 200) {
      last_hw_list_ms_ = now;
      if (sensors_ != nullptr && hw_tool_ == HwToolId::Sensors) sensors_->Tick();
      RefreshHwToolScreen(false);
    }
  } else if (sensors_ != nullptr) {
    sensors_->SetLiveMode(false);
  }
  lv_timer_handler();
}

void UiManager::PostAction(drivers::InputAction action, char ch) {
  if (input_queue_ == nullptr) {
    return;
  }
  UiInputEvent event;
  event.action = action;
  event.ch = ch;
  xQueueSend(input_queue_, &event, 0);
}

void UiManager::SetNrfTelemetry(const modules::NrfTelemetry& telemetry) {
  nrf_telemetry_ = telemetry;
  system_status_.nrf = telemetry;
}

void UiManager::SetSystemStatus(const SystemStatus& status) {
  system_status_ = status;
  nrf_telemetry_ = status.nrf;
}

void UiManager::TintStatusIcon(lv_obj_t* icon, uint32_t color) {
  if (icon == nullptr) return;
  const lv_color_t c = lv_color_hex(color);
  lv_obj_set_style_arc_color(icon, c, LV_PART_MAIN);
  lv_obj_set_style_bg_color(icon, c, 0);
  lv_obj_set_style_line_color(icon, c, 0);
  lv_obj_set_style_border_color(icon, c, 0);
  const uint32_t n = lv_obj_get_child_count(icon);
  for (uint32_t i = 0; i < n; ++i) {
    TintStatusIcon(lv_obj_get_child(icon, i), color);
  }
}

void UiManager::StyleModeIcon(lv_obj_t* icon, bool active, bool warn) {
  if (icon == nullptr) return;
  uint32_t color = 0x405468;
  if (active) color = warn ? 0xFFB84D : 0x00D1FF;
  TintStatusIcon(icon, color);
}

lv_obj_t* UiManager::CreateWifiIcon(lv_obj_t* parent) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 14, 12);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(box, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

  const int sizes[] = {14, 10, 6};
  const int offs[] = {1, 0, -1};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t* arc = lv_arc_create(box);
    lv_obj_set_size(arc, sizes[i], sizes[i]);
    lv_obj_align(arc, LV_ALIGN_BOTTOM_MID, 0, offs[i]);
    lv_arc_set_bg_angles(arc, 210, 330);
    lv_arc_set_angles(arc, 210, 210);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_style(arc, nullptr, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x405468), LV_PART_MAIN);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  }

  lv_obj_t* dot = lv_obj_create(box);
  lv_obj_remove_style_all(dot);
  lv_obj_set_size(dot, 3, 3);
  lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -1);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(0x405468), 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  return box;
}

lv_obj_t* UiManager::CreateRfIcon(lv_obj_t* parent) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 12, 12);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(box, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

  // mast
  lv_obj_t* mast = lv_obj_create(box);
  lv_obj_remove_style_all(mast);
  lv_obj_set_size(mast, 2, 8);
  lv_obj_align(mast, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(mast, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(mast, lv_color_hex(0x405468), 0);
  lv_obj_set_style_radius(mast, 1, 0);

  // tip
  lv_obj_t* tip = lv_obj_create(box);
  lv_obj_remove_style_all(tip);
  lv_obj_set_size(tip, 4, 4);
  lv_obj_align(tip, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(tip, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(tip, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(tip, lv_color_hex(0x405468), 0);

  // side waves
  for (int side = -1; side <= 1; side += 2) {
    lv_obj_t* w1 = lv_obj_create(box);
    lv_obj_remove_style_all(w1);
    lv_obj_set_size(w1, 2, 4);
    lv_obj_align(w1, LV_ALIGN_LEFT_MID, side < 0 ? 1 : 9, -1);
    lv_obj_set_style_bg_opa(w1, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(w1, lv_color_hex(0x405468), 0);
    lv_obj_set_style_radius(w1, 1, 0);

    lv_obj_t* w2 = lv_obj_create(box);
    lv_obj_remove_style_all(w2);
    lv_obj_set_size(w2, 2, 6);
    lv_obj_align(w2, LV_ALIGN_LEFT_MID, side < 0 ? -1 : 11, -1);
    lv_obj_set_style_bg_opa(w2, LV_OPA_40, 0);
    lv_obj_set_style_bg_color(w2, lv_color_hex(0x405468), 0);
    lv_obj_set_style_radius(w2, 1, 0);
  }
  return box;
}

lv_obj_t* UiManager::CreateBtIcon(lv_obj_t* parent) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 10, 14);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(box, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

  // vertical stem
  lv_obj_t* stem = lv_obj_create(box);
  lv_obj_remove_style_all(stem);
  lv_obj_set_size(stem, 2, 12);
  lv_obj_align(stem, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(stem, lv_color_hex(0x405468), 0);
  lv_obj_set_style_radius(stem, 1, 0);

  // upper triangle-ish bar
  lv_obj_t* up = lv_obj_create(box);
  lv_obj_remove_style_all(up);
  lv_obj_set_size(up, 6, 2);
  lv_obj_align(up, LV_ALIGN_TOP_MID, 2, 2);
  lv_obj_set_style_bg_opa(up, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(up, lv_color_hex(0x405468), 0);
  lv_obj_set_style_radius(up, 1, 0);

  // lower bar
  lv_obj_t* dn = lv_obj_create(box);
  lv_obj_remove_style_all(dn);
  lv_obj_set_size(dn, 6, 2);
  lv_obj_align(dn, LV_ALIGN_BOTTOM_MID, 2, -2);
  lv_obj_set_style_bg_opa(dn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dn, lv_color_hex(0x405468), 0);
  lv_obj_set_style_radius(dn, 1, 0);

  // mid diamond dash
  lv_obj_t* mid = lv_obj_create(box);
  lv_obj_remove_style_all(mid);
  lv_obj_set_size(mid, 5, 2);
  lv_obj_align(mid, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_bg_opa(mid, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(mid, lv_color_hex(0x405468), 0);
  lv_obj_set_style_radius(mid, 1, 0);

  return box;
}

bool UiManager::ConsumeSettingsUpdate(services::AppSettings& out) {
  if (!settings_dirty_) return false;
  out = settings_;
  settings_dirty_ = false;
  return true;
}

void UiManager::BuildTheme() {
  lv_style_init(&style_screen_);
  lv_style_set_bg_color(&style_screen_, lv_color_hex(kBgColorHex));
  lv_style_set_bg_opa(&style_screen_, LV_OPA_COVER);

  lv_style_init(&style_logo_);
  lv_style_set_text_color(&style_logo_, lv_color_hex(kPrimaryHex));
  lv_style_set_text_font(&style_logo_, &font_ru_14);
  lv_style_set_text_letter_space(&style_logo_, 2);
  lv_style_set_text_align(&style_logo_, LV_TEXT_ALIGN_CENTER);

  lv_style_init(&style_bar_bg_);
  lv_style_set_radius(&style_bar_bg_, 10);
  lv_style_set_bg_color(&style_bar_bg_, lv_color_hex(kMutedHex));
  lv_style_set_bg_opa(&style_bar_bg_, LV_OPA_80);
  lv_style_set_border_color(&style_bar_bg_, lv_color_hex(kSecondaryHex));
  lv_style_set_border_width(&style_bar_bg_, 1);

  lv_style_init(&style_bar_indic_);
  lv_style_set_bg_color(&style_bar_indic_, lv_color_hex(kPrimaryHex));
  lv_style_set_bg_grad_color(&style_bar_indic_, lv_color_hex(kSecondaryHex));
  lv_style_set_bg_grad_dir(&style_bar_indic_, LV_GRAD_DIR_HOR);

  lv_style_init(&style_status_);
  lv_style_set_bg_color(&style_status_, lv_color_hex(0x0D1420));
  lv_style_set_bg_opa(&style_status_, LV_OPA_COVER);
  lv_style_set_border_side(&style_status_, LV_BORDER_SIDE_BOTTOM);
  lv_style_set_border_color(&style_status_, lv_color_hex(0x1E2C40));
  lv_style_set_border_width(&style_status_, 1);
  lv_style_set_pad_hor(&style_status_, 6);

  lv_style_init(&style_item_);
  lv_style_set_bg_opa(&style_item_, LV_OPA_TRANSP);
  lv_style_set_radius(&style_item_, 4);
  lv_style_set_pad_left(&style_item_, 10);
  lv_style_set_pad_right(&style_item_, 4);

  lv_style_init(&style_item_selected_);
  lv_style_set_text_color(&style_item_selected_, lv_color_hex(0xEAFBFF));

  lv_style_init(&style_cursor_);
  lv_style_set_radius(&style_cursor_, 4);
  lv_style_set_bg_color(&style_cursor_, lv_color_hex(0x12384A));
  lv_style_set_bg_opa(&style_cursor_, LV_OPA_70);
  lv_style_set_border_side(&style_cursor_, LV_BORDER_SIDE_LEFT);
  lv_style_set_border_width(&style_cursor_, 2);
  lv_style_set_border_color(&style_cursor_, lv_color_hex(kPrimaryHex));
}

void UiManager::BuildBootScreen() {
  boot_screen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(boot_screen_);
  lv_obj_add_style(boot_screen_, &style_screen_, 0);

  logo_label_ = lv_label_create(boot_screen_);
  lv_label_set_text(logo_label_, "AxiomOS");
  lv_obj_add_style(logo_label_, &style_logo_, 0);
  lv_obj_align(logo_label_, LV_ALIGN_CENTER, 0, -22);
  lv_obj_set_style_opa(logo_label_, LV_OPA_0, 0);

  lv_obj_t* subtitle = lv_label_create(boot_screen_);
  lv_label_set_text(subtitle, "Редакция Cardputer");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0x8A96A8), 0);
  lv_obj_set_style_text_font(subtitle, &font_ru_14, 0);
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 8);

  loading_bar_ = lv_bar_create(boot_screen_);
  lv_obj_set_size(loading_bar_, 180, 10);
  lv_obj_align(loading_bar_, LV_ALIGN_CENTER, 0, 46);
  lv_obj_add_style(loading_bar_, &style_bar_bg_, LV_PART_MAIN);
  lv_obj_add_style(loading_bar_, &style_bar_indic_, LV_PART_INDICATOR);
  lv_bar_set_range(loading_bar_, 0, 100);
  lv_bar_set_value(loading_bar_, 0, LV_ANIM_OFF);

  lv_scr_load(boot_screen_);
}

void UiManager::AnimateBootLogo() {
  lv_anim_t fade;
  lv_anim_init(&fade);
  lv_anim_set_var(&fade, logo_label_);
  lv_anim_set_values(&fade, LV_OPA_0, LV_OPA_COVER);
  lv_anim_set_time(&fade, 700);
  lv_anim_set_exec_cb(&fade, BootFadeAnimationCb);
  lv_anim_start(&fade);

  lv_anim_t progress;
  lv_anim_init(&progress);
  lv_anim_set_var(&progress, loading_bar_);
  lv_anim_set_values(&progress, 0, 100);
  lv_anim_set_time(&progress, 1600);
  lv_anim_set_path_cb(&progress, lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&progress, BootProgressAnimationCb);
  lv_anim_start(&progress);

  lv_screen_load_anim(menu_screen_, LV_SCR_LOAD_ANIM_FADE_ON, 320, 1700, false);
}

void UiManager::BuildMenuScreen(bool forward) {
  menu_screen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(menu_screen_);
  lv_obj_add_style(menu_screen_, &style_screen_, 0);

  lv_obj_t* status_bar = lv_obj_create(menu_screen_);
  lv_obj_remove_style_all(status_bar);
  lv_obj_add_style(status_bar, &style_status_, 0);
  lv_obj_set_size(status_bar, LV_PCT(100), 18);
  lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);

  status_label_ = lv_label_create(status_bar);
  lv_label_set_text(status_label_, "AxiomOS");
  lv_obj_set_style_text_font(status_label_, &font_ru_14, 0);
  lv_obj_set_style_text_color(status_label_, lv_color_hex(kPrimaryHex), 0);
  lv_obj_align(status_label_, LV_ALIGN_LEFT_MID, 0, 0);

  icon_bat_ = lv_label_create(status_bar);
  lv_label_set_text(icon_bat_, "--%");
  lv_obj_set_style_text_font(icon_bat_, &font_ru_14, 0);
  lv_obj_set_style_text_color(icon_bat_, lv_color_hex(0x405468), 0);
  lv_obj_align(icon_bat_, LV_ALIGN_RIGHT_MID, -2, 0);

  icon_bt_ = CreateBtIcon(status_bar);
  lv_obj_align_to(icon_bt_, icon_bat_, LV_ALIGN_OUT_LEFT_MID, -7, 0);

  icon_rf_ = CreateRfIcon(status_bar);
  lv_obj_align_to(icon_rf_, icon_bt_, LV_ALIGN_OUT_LEFT_MID, -7, 0);

  icon_wifi_ = CreateWifiIcon(status_bar);
  lv_obj_align_to(icon_wifi_, icon_rf_, LV_ALIGN_OUT_LEFT_MID, -7, 0);

  breadcrumb_label_ = lv_label_create(menu_screen_);
  lv_obj_set_style_text_font(breadcrumb_label_, &font_ru_14, 0);
  lv_obj_set_style_text_color(breadcrumb_label_, lv_color_hex(0xD8E6F5), 0);
  lv_obj_align(breadcrumb_label_, LV_ALIGN_TOP_LEFT, 8, 22);

  title_line_ = lv_obj_create(menu_screen_);
  lv_obj_remove_style_all(title_line_);
  lv_obj_set_size(title_line_, 48, 2);
  lv_obj_set_style_bg_color(title_line_, lv_color_hex(kPrimaryHex), 0);
  lv_obj_set_style_bg_opa(title_line_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(title_line_, 1, 0);
  lv_obj_align(title_line_, LV_ALIGN_TOP_LEFT, 8, 38);

  menu_container_ = lv_obj_create(menu_screen_);
  lv_obj_remove_style_all(menu_container_);
  lv_obj_set_style_bg_opa(menu_container_, LV_OPA_TRANSP, 0);
  lv_obj_set_size(menu_container_, 224, 84);
  lv_obj_align(menu_container_, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_set_style_clip_corner(menu_container_, true, 0);

  selection_cursor_ = lv_obj_create(menu_container_);
  lv_obj_remove_style_all(selection_cursor_);
  lv_obj_add_style(selection_cursor_, &style_cursor_, 0);
  lv_obj_set_size(selection_cursor_, 224, kRowH);
  lv_obj_align(selection_cursor_, LV_ALIGN_TOP_LEFT, 0, 0);

  for (int i = 0; i < 6; ++i) {
    menu_rows_[i] = lv_obj_create(menu_container_);
    lv_obj_remove_style_all(menu_rows_[i]);
    lv_obj_add_style(menu_rows_[i], &style_item_, 0);
    lv_obj_set_size(menu_rows_[i], 224, kRowH);
    lv_obj_align(menu_rows_[i], LV_ALIGN_TOP_LEFT, 0, i * kRowStep);
    lv_obj_clear_flag(menu_rows_[i], LV_OBJ_FLAG_SCROLLABLE);

    menu_texts_[i] = lv_label_create(menu_rows_[i]);
    lv_obj_set_style_text_font(menu_texts_[i], &font_ru_14, 0);
    lv_obj_set_style_text_color(menu_texts_[i], lv_color_hex(0x9EB0C4), 0);
    lv_obj_align(menu_texts_[i], LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_long_mode(menu_texts_[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_width(menu_texts_[i], 210);
  }

  hint_label_ = lv_label_create(menu_screen_);
  lv_label_set_text(hint_label_, ";/.  Enter  Del");
  lv_obj_set_style_text_font(hint_label_, &font_ru_14, 0);
  lv_obj_set_style_text_color(hint_label_, lv_color_hex(0x62768C), 0);
  lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

  RefreshStatusBar();
  RefreshMenuList(true);

  if (lv_screen_active() == boot_screen_) {
    return;
  }
  lv_screen_load_anim(menu_screen_,
                      forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT, 220, 0,
                      true);
}

void UiManager::AnimateSelectionCursor(int index) {
  if (selection_cursor_ == nullptr) return;
  const int32_t target_y = index * kRowStep;

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, selection_cursor_);
  lv_anim_set_values(&anim, lv_obj_get_y(selection_cursor_), target_y);
  lv_anim_set_time(&anim, 140);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim, [](void* obj, int32_t v) {
    lv_obj_set_y(static_cast<lv_obj_t*>(obj), v);
  });
  lv_anim_start(&anim);

  lv_anim_t pulse;
  lv_anim_init(&pulse);
  lv_anim_set_var(&pulse, selection_cursor_);
  lv_anim_set_values(&pulse, LV_OPA_40, LV_OPA_COVER);
  lv_anim_set_time(&pulse, 160);
  lv_anim_set_path_cb(&pulse, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&pulse, AnimOpa);
  lv_anim_start(&pulse);
}

void UiManager::AnimateItemsIn(int count, bool forward) {
  const int32_t from_x = forward ? 18 : -18;
  for (int i = 0; i < 6; ++i) {
    if (i >= count) continue;

    lv_obj_set_style_opa(menu_rows_[i], LV_OPA_0, 0);
    lv_obj_set_style_translate_x(menu_rows_[i], from_x, 0);

    lv_anim_t slide;
    lv_anim_init(&slide);
    lv_anim_set_var(&slide, menu_rows_[i]);
    lv_anim_set_values(&slide, from_x, 0);
    lv_anim_set_time(&slide, 180);
    lv_anim_set_delay(&slide, static_cast<uint32_t>(i * 35));
    lv_anim_set_path_cb(&slide, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&slide, AnimTranslateX);
    lv_anim_start(&slide);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, menu_rows_[i]);
    lv_anim_set_values(&fade, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&fade, 180);
    lv_anim_set_delay(&fade, static_cast<uint32_t>(i * 35));
    lv_anim_set_exec_cb(&fade, AnimOpa);
    lv_anim_start(&fade);
  }

  if (breadcrumb_label_) {
    lv_obj_set_style_opa(breadcrumb_label_, LV_OPA_30, 0);
    lv_anim_t bf;
    lv_anim_init(&bf);
    lv_anim_set_var(&bf, breadcrumb_label_);
    lv_anim_set_values(&bf, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_time(&bf, 220);
    lv_anim_set_exec_cb(&bf, AnimOpa);
    lv_anim_start(&bf);
  }

  if (title_line_) {
    lv_obj_set_width(title_line_, 8);
    lv_anim_t grow;
    lv_anim_init(&grow);
    lv_anim_set_var(&grow, title_line_);
    lv_anim_set_values(&grow, 8, 56);
    lv_anim_set_time(&grow, 260);
    lv_anim_set_path_cb(&grow, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&grow, [](void* obj, int32_t v) {
      lv_obj_set_width(static_cast<lv_obj_t*>(obj), v);
    });
    lv_anim_start(&grow);
  }
}

void UiManager::RefreshStatusBar() {
  if (status_label_ == nullptr || icon_wifi_ == nullptr) return;

  lv_label_set_text(status_label_, "AxiomOS");

  const bool wifi_on = system_status_.wifi.connected;
  const bool wifi_warn = !wifi_on && system_status_.wifi.networks_found > 0;
  StyleModeIcon(icon_wifi_, wifi_on || wifi_warn, wifi_warn);
  StyleModeIcon(icon_rf_, system_status_.nrf.present);
  StyleModeIcon(icon_bt_, system_status_.bt.initialized);

  if (icon_bat_ != nullptr) {
    int32_t pct = -1;
    bool charging = false;
    if (sensors_ != nullptr) {
      const auto s = sensors_->GetTelemetry();
      pct = s.battery_percent;
      charging = s.charging;
      // fallback from voltage if % unknown
      if (pct < 0 && s.battery_mv > 0) {
        // rough LiPo map 3.3V..4.2V
        const float v = s.battery_mv / 1000.0f;
        pct = static_cast<int32_t>((v - 3.30f) / (4.20f - 3.30f) * 100.0f);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
      }
    }
    if (pct >= 0) {
      lv_label_set_text_fmt(icon_bat_, "%ld%%", static_cast<long>(pct));
      uint32_t color = 0x5CFF9A;
      if (charging) color = kPrimaryHex;
      else if (pct <= 15) color = 0xFF6B6B;
      else if (pct <= 30) color = 0xFFB84D;
      else if (pct <= 60) color = 0x9EB0C4;
      lv_obj_set_style_text_color(icon_bat_, lv_color_hex(color), 0);
    } else {
      lv_label_set_text(icon_bat_, "--%");
      lv_obj_set_style_text_color(icon_bat_, lv_color_hex(0x405468), 0);
    }
    // keep cluster right-aligned when width changes
    lv_obj_align(icon_bat_, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_align_to(icon_bt_, icon_bat_, LV_ALIGN_OUT_LEFT_MID, -7, 0);
    lv_obj_align_to(icon_rf_, icon_bt_, LV_ALIGN_OUT_LEFT_MID, -7, 0);
    lv_obj_align_to(icon_wifi_, icon_rf_, LV_ALIGN_OUT_LEFT_MID, -7, 0);
  }

  if (breadcrumb_label_ == nullptr) return;

  if (in_settings_screen_) {
    lv_label_set_text(breadcrumb_label_, "Система / Настройки");
  } else if (in_fs_browser_) {
    lv_label_set_text(breadcrumb_label_, "Файлы");
  } else if (in_about_screen_) {
    lv_label_set_text(breadcrumb_label_, "О системе");
  } else if (radio_tool_ != RadioToolId::None) {
    switch (radio_tool_) {
      case RadioToolId::Scanner:
        lv_label_set_text(breadcrumb_label_, "Сканер спектра");
        break;
      case RadioToolId::Monitor:
        lv_label_set_text(breadcrumb_label_, "Монитор пакетов");
        break;
      case RadioToolId::Manager:
        lv_label_set_text(breadcrumb_label_, "Менеджер nRF24");
        break;
      default:
        lv_label_set_text(breadcrumb_label_, "Радио");
        break;
    }
  } else if (in_sensor_graph_) {
    lv_label_set_text(breadcrumb_label_,
                      sensor_graph_gyro_ ? "Гироскоп" : "Акселерометр");
  } else if (hw_tool_ != HwToolId::None) {
    switch (hw_tool_) {
      case HwToolId::Gpio:
        lv_label_set_text(breadcrumb_label_, "Монитор GPIO");
        break;
      case HwToolId::I2c:
        lv_label_set_text(breadcrumb_label_, "Сканер I2C");
        break;
      case HwToolId::Sensors:
        lv_label_set_text(breadcrumb_label_, "Датчики");
        break;
      default:
        lv_label_set_text(breadcrumb_label_, "Железо");
        break;
    }
  } else if (net_tool_ != NetToolId::None) {
    switch (net_tool_) {
      case NetToolId::Websocket:
        lv_label_set_text(breadcrumb_label_, "Вебсокет");
        break;
      case NetToolId::Http:
        lv_label_set_text(breadcrumb_label_, "HTTP клиент");
        break;
      case NetToolId::Tcp:
        lv_label_set_text(breadcrumb_label_, "TCP клиент");
        break;
      case NetToolId::Ping:
        lv_label_set_text(breadcrumb_label_, "Пинг / DNS");
        break;
      case NetToolId::Info:
        lv_label_set_text(breadcrumb_label_, "Сеть / IP");
        break;
      default:
        lv_label_set_text(breadcrumb_label_, "Сеть");
        break;
    }
  } else if (in_mqtt_screen_) {
    lv_label_set_text(breadcrumb_label_, "MQTT клиент");
  } else if (in_wifi_password_) {
    lv_label_set_text(breadcrumb_label_, "Пароль WiFi");
  } else if (in_wifi_scanner_) {
    if (wifi_ != nullptr && wifi_->IsScanning()) {
      lv_label_set_text(breadcrumb_label_, "Сканер WiFi...");
    } else if (wifi_ != nullptr) {
      lv_label_set_text_fmt(breadcrumb_label_, "WiFi сети: %u", wifi_->NetworkCount());
    } else {
      lv_label_set_text(breadcrumb_label_, "Сканер WiFi");
    }
  } else if (in_submenu_) {
    lv_label_set_text_fmt(breadcrumb_label_, "%s", kSubmenus[active_section_].title);
  } else {
    lv_label_set_text(breadcrumb_label_, "Главное меню");
  }

  if (hint_label_ == nullptr) return;
  if (in_submenu_ && active_section_ == 0 && nrf_telemetry_.present) {
    lv_label_set_text_fmt(hint_label_, "RF %u  %u%%  %s", nrf_telemetry_.current_channel,
                          nrf_telemetry_.activity_percent, nrf_telemetry_.tx_ok ? "OK" : "--");
  }
}

void UiManager::RefreshMenuList(bool animate_in) {
  if (in_wifi_password_) {
    RefreshWifiPasswordScreen();
    return;
  }
  if (in_wifi_scanner_) {
    RefreshWifiScannerList(animate_in);
    return;
  }
  if (in_mqtt_screen_) {
    RefreshMqttScreen(animate_in);
    return;
  }
  if (net_tool_ != NetToolId::None) {
    RefreshNetToolScreen(animate_in);
    return;
  }
  if (radio_tool_ != RadioToolId::None) {
    RefreshRadioScreen(animate_in);
    return;
  }
  if (hw_tool_ != HwToolId::None) {
    RefreshHwToolScreen(animate_in);
    return;
  }
  if (in_fs_browser_) {
    RefreshFsBrowser(animate_in);
    return;
  }
  if (in_about_screen_) {
    RefreshAboutScreen(animate_in);
    return;
  }

  int focus = selected_index_;
  int count = 0;

  if (in_settings_screen_) {
    const char* theme = settings_.theme == services::ThemeMode::Cyberpunk ? "Киберпанк" : "Тёмная";
    char lines[6][40];
    snprintf(lines[0], sizeof(lines[0]), "Яркость      %u", settings_.brightness);
    snprintf(lines[1], sizeof(lines[1]), "Громкость    %u", settings_.volume);
    snprintf(lines[2], sizeof(lines[2]), "Тема         %s", theme);
    snprintf(lines[3], sizeof(lines[3]), "RF канал     %u", settings_.rf_channel);
    snprintf(lines[4], sizeof(lines[4]), "RF мощность  %u", settings_.rf_power);
    snprintf(lines[5], sizeof(lines[5]), "Назад");
    count = 6;
    focus = settings_index_;
    if (focus < settings_scroll_) settings_scroll_ = focus;
    if (focus >= settings_scroll_ + kVisibleRows) settings_scroll_ = focus - kVisibleRows + 1;

    for (int row = 0; row < 6; ++row) {
      const int idx = settings_scroll_ + row;
      if (row >= kVisibleRows || idx >= count) {
        lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(menu_texts_[row], lines[idx]);
      lv_obj_set_style_text_color(menu_texts_[row],
                                  lv_color_hex(idx == focus ? 0xEAFBFF : 0x9EB0C4), 0);
    }
    focus = settings_index_ - settings_scroll_;
    lv_label_set_text(hint_label_,
                      settings_edit_mode_ ? ";/. изм  Enter ок" : ";/.  Enter  Del");
    count = kVisibleRows;
  } else {
    const MenuNode& node = in_submenu_ ? kSubmenus[active_section_] : kRootMenu;
    count = node.item_count;
    focus = selected_index_;
    if (focus < menu_scroll_) menu_scroll_ = focus;
    if (focus >= menu_scroll_ + kVisibleRows) menu_scroll_ = focus - kVisibleRows + 1;

    int shown = 0;
    for (int row = 0; row < 6; ++row) {
      const int idx = menu_scroll_ + row;
      if (row >= kVisibleRows || idx >= node.item_count) {
        lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      ++shown;
      lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(menu_texts_[row], node.items[idx]);
      lv_obj_set_style_text_color(menu_texts_[row],
                                  lv_color_hex(idx == focus ? 0xEAFBFF : 0x9EB0C4), 0);
    }
    focus = selected_index_ - menu_scroll_;
    count = shown;
  }

  if (selection_cursor_) {
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) {
      lv_obj_set_y(selection_cursor_, focus * kRowStep);
      lv_obj_set_style_opa(selection_cursor_, LV_OPA_COVER, 0);
    } else {
      AnimateSelectionCursor(focus);
    }
  }

  if (animate_in) {
    AnimateItemsIn(count, true);
  }
}

void UiManager::RefreshWifiScannerList(bool animate_in) {
  LayoutListRows(kRowStep);

  if (wifi_ == nullptr) {
    for (int i = 0; i < 6; ++i) lv_obj_add_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(hint_label_, "WiFi недоступен");
    return;
  }

  if (wifi_->IsScanning() && wifi_->NetworkCount() == 0) {
    for (int i = 0; i < 6; ++i) {
      if (i == 0) {
        lv_obj_remove_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(menu_texts_[i], "Сканирование...");
        lv_obj_set_style_text_color(menu_texts_[i], lv_color_hex(0x7DF0FF), 0);
      } else {
        lv_obj_add_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (selection_cursor_) {
      lv_obj_set_y(selection_cursor_, 0);
      lv_obj_set_height(selection_cursor_, kRowH);
      lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(hint_label_, "Подождите...");
    if (animate_in) AnimateItemsIn(1, true);
    return;
  }

  const uint8_t total = wifi_->NetworkCount();
  if (total == 0) {
    for (int i = 0; i < 6; ++i) {
      if (i == 0) {
        lv_obj_remove_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(menu_texts_[i], "Сети не найдены");
        lv_obj_set_style_text_color(menu_texts_[i], lv_color_hex(0x9EB0C4), 0);
      } else {
        lv_obj_add_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    wifi_selected_ = 0;
    wifi_scroll_ = 0;
    if (selection_cursor_) {
      lv_obj_set_y(selection_cursor_, 0);
      lv_obj_set_height(selection_cursor_, kRowH);
    }
    lv_label_set_text(hint_label_, "R обновить   Del назад");
    if (animate_in) AnimateItemsIn(1, true);
    return;
  }

  if (wifi_selected_ >= total) wifi_selected_ = total - 1;
  if (wifi_selected_ < 0) wifi_selected_ = 0;

  constexpr int kVisible = 4;
  if (wifi_selected_ < wifi_scroll_) wifi_scroll_ = wifi_selected_;
  if (wifi_selected_ >= wifi_scroll_ + kVisible) wifi_scroll_ = wifi_selected_ - kVisible + 1;

  char line[40];
  char name[16];
  int shown = 0;
  for (int row = 0; row < 6; ++row) {
    const int idx = wifi_scroll_ + row;
    if (row >= kVisible || idx >= total) {
      lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    ++shown;
    const auto& net = wifi_->NetworkAt(static_cast<uint8_t>(idx));
    TruncateText(name, sizeof(name), net.ssid, 12);
    // Коротко: сигнал + имя + lock, без RSSI в одной строке
    snprintf(line, sizeof(line), "%s  %s%s", SignalBars(net.rssi), name,
             net.encrypted ? " *" : "");
    lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(menu_rows_[row], kRowH);
    lv_label_set_text(menu_texts_[row], line);
    lv_label_set_long_mode(menu_texts_[row], LV_LABEL_LONG_CLIP);
    lv_obj_set_width(menu_texts_[row], 200);
    lv_obj_set_style_text_color(menu_texts_[row],
                                lv_color_hex(idx == wifi_selected_ ? 0xEAFBFF : 0x9EB0C4), 0);
  }

  const int cursor_row = wifi_selected_ - wifi_scroll_;
  if (selection_cursor_) {
    lv_obj_set_height(selection_cursor_, kRowH);
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) {
      lv_obj_set_y(selection_cursor_, cursor_row * kRowStep);
    } else {
      AnimateSelectionCursor(cursor_row);
    }
  }

  const auto& sel = wifi_->NetworkAt(static_cast<uint8_t>(wifi_selected_));
  lv_label_set_text_fmt(hint_label_, "%ld дБм  кан.%u  %s", static_cast<long>(sel.rssi),
                        sel.channel, sel.encrypted ? "WPA" : "открытая");
  if (animate_in) AnimateItemsIn(shown, true);
}

const char* UiManager::SignalBars(int32_t rssi) {
  if (rssi > -55) return "||||";
  if (rssi > -67) return "||| ";
  if (rssi > -80) return "||  ";
  return "|   ";
}

void UiManager::TruncateText(char* dst, size_t dst_size, const char* src, size_t max_chars) {
  if (dst == nullptr || dst_size == 0) return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  size_t len = strlen(src);
  if (len <= max_chars) {
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
    return;
  }
  if (dst_size < 4 || max_chars < 3) {
    dst[0] = '\0';
    return;
  }
  const size_t keep = max_chars - 2;
  memcpy(dst, src, keep);
  dst[keep] = '.';
  dst[keep + 1] = '.';
  dst[keep + 2] = '\0';
}

void UiManager::LayoutListRows(int32_t step) {
  for (int i = 0; i < 6; ++i) {
    if (menu_rows_[i] == nullptr) continue;
    lv_obj_set_height(menu_rows_[i], kRowH);
    lv_obj_set_y(menu_rows_[i], i * step);
    if (menu_texts_[i]) {
      lv_obj_align(menu_texts_[i], LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_set_width(menu_texts_[i], 200);
      lv_label_set_long_mode(menu_texts_[i], LV_LABEL_LONG_CLIP);
    }
  }
  if (selection_cursor_) {
    lv_obj_set_height(selection_cursor_, kRowH);
  }
}

void UiManager::RefreshWifiPasswordScreen() {
  // Разреженные строки, без наложений
  constexpr int32_t kPassStep = 22;
  LayoutListRows(kPassStep);

  for (int i = 0; i < 6; ++i) lv_obj_add_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);

  lv_obj_remove_flag(menu_rows_[0], LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(menu_rows_[1], LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(menu_rows_[2], LV_OBJ_FLAG_HIDDEN);

  char ssid_short[18];
  TruncateText(ssid_short, sizeof(ssid_short), wifi_target_ssid_, 14);
  lv_label_set_text_fmt(menu_texts_[0], "Сеть: %s", ssid_short);
  lv_obj_set_style_text_color(menu_texts_[0], lv_color_hex(0x7DF0FF), 0);

  char masked[36];
  if (wifi_password_len_ == 0) {
    strncpy(masked, "(пусто)", sizeof(masked));
  } else {
    uint8_t n = wifi_password_len_;
    if (n > 18) n = 18;
    for (uint8_t i = 0; i < n; ++i) masked[i] = '*';
    masked[n] = '\0';
  }
  lv_label_set_text_fmt(menu_texts_[1], "Пароль: %s", masked);
  lv_obj_set_style_text_color(menu_texts_[1], lv_color_hex(0xEAFBFF), 0);

  const auto st = wifi_ != nullptr ? wifi_->ConnectState() : modules::WifiConnectState::Idle;
  const char* status = "Введите пароль";
  if (!wifi_target_encrypted_) status = "Открытая сеть";
  if (st == modules::WifiConnectState::Connecting) status = "Подключение...";
  if (st == modules::WifiConnectState::Connected) status = "Подключено!";
  if (st == modules::WifiConnectState::Failed) status = "Ошибка входа";
  lv_label_set_text(menu_texts_[2], status);
  lv_obj_set_style_text_color(menu_texts_[2],
                              lv_color_hex(st == modules::WifiConnectState::Failed ? 0xFF6B6B
                                            : st == modules::WifiConnectState::Connected ? 0x5CFF9A
                                                                                        : 0x9EB0C4),
                              0);

  // Курсор только на строке пароля, не перекрывает SSID/статус
  if (selection_cursor_) {
    lv_obj_set_y(selection_cursor_, kPassStep);
    lv_obj_set_height(selection_cursor_, kRowH);
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
  }

  lv_label_set_text(hint_label_, "Enter OK   Del стереть   ` назад");
}

void UiManager::OpenWifiScanner() {
  in_wifi_scanner_ = true;
  in_wifi_password_ = false;
  wifi_selected_ = 0;
  wifi_scroll_ = 0;
  last_wifi_count_ = 0;
  last_wifi_scanning_ = true;
  LayoutListRows(kRowStep);
  if (keyboard_) keyboard_->SetTextCapture(false);
  if (wifi_ != nullptr) wifi_->SetScannerActive(true);
  RefreshWifiScannerList(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseWifiScanner() {
  in_wifi_scanner_ = false;
  in_wifi_password_ = false;
  LayoutListRows(kRowStep);
  if (keyboard_) keyboard_->SetTextCapture(false);
  if (wifi_ != nullptr) wifi_->SetScannerActive(false);
  RefreshMenuList(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::OpenWifiPassword() {
  if (wifi_ == nullptr) return;
  if (wifi_->NetworkCount() == 0) return;

  const auto& net = wifi_->NetworkAt(static_cast<uint8_t>(wifi_selected_));
  strncpy(wifi_target_ssid_, net.ssid, sizeof(wifi_target_ssid_) - 1);
  wifi_target_ssid_[sizeof(wifi_target_ssid_) - 1] = '\0';
  wifi_target_encrypted_ = net.encrypted;
  wifi_password_[0] = '\0';
  wifi_password_len_ = 0;
  last_connect_state_ = modules::WifiConnectState::Idle;
  in_wifi_password_ = true;

  if (!wifi_target_encrypted_) {
    BeginWifiConnect();
    RefreshWifiPasswordScreen();
    return;
  }

  if (keyboard_) keyboard_->SetTextCapture(true);
  RefreshWifiPasswordScreen();
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseWifiPassword(bool back_to_list) {
  in_wifi_password_ = false;
  if (keyboard_) keyboard_->SetTextCapture(false);
  wifi_password_[0] = '\0';
  wifi_password_len_ = 0;
  LayoutListRows(kRowStep);
  if (back_to_list) {
    RefreshWifiScannerList(true);
  }
}

void UiManager::BeginWifiConnect() {
  if (wifi_ == nullptr) return;
  last_connect_state_ = modules::WifiConnectState::Connecting;
  wifi_->Connect(wifi_target_ssid_, wifi_target_encrypted_ ? wifi_password_ : "");
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::HandleWifiPasswordInput(const UiInputEvent& event) {
  using A = drivers::InputAction;
  switch (event.action) {
    case A::Char:
      if (wifi_password_len_ < sizeof(wifi_password_) - 1 && event.ch != 0) {
        wifi_password_[wifi_password_len_++] = event.ch;
        wifi_password_[wifi_password_len_] = '\0';
        if (audio_) audio_->Play(drivers::SoundId::KeyClick);
        RefreshWifiPasswordScreen();
      }
      break;
    case A::DeleteChar:
      if (wifi_password_len_ > 0) {
        wifi_password_[--wifi_password_len_] = '\0';
        if (audio_) audio_->Play(drivers::SoundId::KeyClick);
        RefreshWifiPasswordScreen();
      } else {
        CloseWifiPassword(true);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      }
      break;
    case A::Back:
      CloseWifiPassword(true);
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      break;
    case A::Select:
      if (wifi_ != nullptr && wifi_->ConnectState() == modules::WifiConnectState::Connected) {
        CloseWifiScanner();
        break;
      }
      if (wifi_target_encrypted_ && wifi_password_len_ == 0) {
        if (audio_) audio_->Play(drivers::SoundId::Error);
        break;
      }
      BeginWifiConnect();
      RefreshWifiPasswordScreen();
      break;
    default:
      break;
  }
}

void UiManager::HandleWifiScannerInput(const UiInputEvent& event) {
  if (in_wifi_password_) {
    HandleWifiPasswordInput(event);
    return;
  }
  if (wifi_ == nullptr) {
    if (event.action == drivers::InputAction::Back) CloseWifiScanner();
    return;
  }

  const uint8_t total = wifi_->NetworkCount();
  switch (event.action) {
    case drivers::InputAction::Up:
      if (total == 0) break;
      wifi_selected_ = (wifi_selected_ - 1 + total) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshWifiScannerList(false);
      break;
    case drivers::InputAction::Down:
      if (total == 0) break;
      wifi_selected_ = (wifi_selected_ + 1) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshWifiScannerList(false);
      break;
    case drivers::InputAction::Select:
      if (total == 0) {
        wifi_->StartScan();
        last_wifi_scanning_ = true;
        RefreshWifiScannerList(false);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      } else {
        OpenWifiPassword();
      }
      break;
    case drivers::InputAction::Rescan:
      wifi_->StartScan();
      last_wifi_scanning_ = true;
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      RefreshWifiScannerList(false);
      break;
    case drivers::InputAction::Back:
      CloseWifiScanner();
      break;
    default:
      break;
  }
}

void UiManager::HandleInput(const UiInputEvent& event) {
  if (radio_tool_ != RadioToolId::None) {
    HandleRadioInput(event);
    RefreshStatusBar();
    return;
  }
  if (in_sensor_graph_) {
    if (event.action == drivers::InputAction::Back) {
      CloseSensorGraph();
    }
    RefreshStatusBar();
    return;
  }
  if (in_fs_browser_) {
    HandleFsBrowserInput(event);
    RefreshStatusBar();
    return;
  }
  if (in_about_screen_) {
    HandleAboutInput(event.action);
    RefreshStatusBar();
    return;
  }
  if (hw_tool_ != HwToolId::None) {
    HandleHwToolInput(event);
    RefreshStatusBar();
    return;
  }
  if (net_tool_ != NetToolId::None) {
    HandleNetToolInput(event);
    RefreshStatusBar();
    return;
  }
  if (in_mqtt_screen_) {
    HandleMqttInput(event);
    RefreshStatusBar();
    return;
  }
  if (in_wifi_scanner_ || in_wifi_password_) {
    HandleWifiScannerInput(event);
    RefreshStatusBar();
    return;
  }
  if (in_settings_screen_) {
    HandleSettingsInput(event.action);
    RefreshStatusBar();
    RefreshMenuList(false);
    return;
  }

  const drivers::InputAction action = event.action;
  const int max_items = in_submenu_ ? kSubmenus[active_section_].item_count : kRootMenu.item_count;
  switch (action) {
    case drivers::InputAction::Up:
      selected_index_ = (selected_index_ - 1 + max_items) % max_items;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshMenuList(false);
      break;
    case drivers::InputAction::Down:
      selected_index_ = (selected_index_ + 1) % max_items;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshMenuList(false);
      break;
    case drivers::InputAction::Select:
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      SelectCurrentItem();
      break;
    case drivers::InputAction::Back:
      if (in_submenu_) {
        in_submenu_ = false;
        selected_index_ = active_section_;
        menu_scroll_ = 0;
        BuildMenuScreen(false);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      } else if (audio_) {
        audio_->Play(drivers::SoundId::Error);
      }
      break;
    case drivers::InputAction::QuickWireless:
      in_submenu_ = true;
      active_section_ = 0;
      selected_index_ = 0;
      menu_scroll_ = 0;
      BuildMenuScreen(true);
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      break;
    case drivers::InputAction::QuickNetwork:
      in_submenu_ = true;
      active_section_ = 1;
      selected_index_ = 0;
      menu_scroll_ = 0;
      BuildMenuScreen(true);
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      break;
    case drivers::InputAction::QuickHardware:
      in_submenu_ = true;
      active_section_ = 2;
      selected_index_ = 0;
      menu_scroll_ = 0;
      BuildMenuScreen(true);
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      break;
    case drivers::InputAction::QuickSystem:
      in_submenu_ = true;
      active_section_ = 3;
      selected_index_ = 0;
      menu_scroll_ = 0;
      BuildMenuScreen(true);
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      break;
    case drivers::InputAction::None:
    default:
      break;
  }

  RefreshStatusBar();
}

void UiManager::SelectCurrentItem() {
  if (!in_submenu_) {
    active_section_ = selected_index_;
    selected_index_ = 0;
    menu_scroll_ = 0;
    in_submenu_ = true;
    BuildMenuScreen(true);
    if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
    return;
  }

  // Радио
  if (active_section_ == 0) {
    switch (selected_index_) {
      case 0:
        OpenRadioTool(RadioToolId::Scanner);
        return;
      case 1:
        OpenRadioTool(RadioToolId::Monitor);
        return;
      case 2:
        OpenRadioTool(RadioToolId::Manager);
        return;
      default:
        break;
    }
  }

  // Сеть
  if (active_section_ == 1) {
    switch (selected_index_) {
      case 0:
        OpenWifiScanner();
        return;
      case 1:
        OpenMqttClient();
        return;
      case 2:
        OpenNetTool(NetToolId::Websocket);
        return;
      case 3:
        OpenNetTool(NetToolId::Http);
        return;
      case 4:
        OpenNetTool(NetToolId::Tcp);
        return;
      case 5:
        OpenNetTool(NetToolId::Ping);
        return;
      case 6:
        OpenNetTool(NetToolId::Info);
        return;
      default:
        break;
    }
  }

  // Железо
  if (active_section_ == 2) {
    switch (selected_index_) {
      case 0:
        OpenHwTool(HwToolId::Gpio);
        return;
      case 1:
        OpenHwTool(HwToolId::I2c);
        return;
      case 2:
        OpenHwTool(HwToolId::Sensors);
        return;
      default:
        break;
    }
  }

  // Система
  if (active_section_ == 3) {
    switch (selected_index_) {
      case 0:
        in_settings_screen_ = true;
        settings_index_ = 0;
        settings_scroll_ = 0;
        settings_edit_mode_ = false;
        RefreshMenuList(true);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        return;
      case 1:
        OpenFsBrowser();
        return;
      case 2:
        OpenAbout();
        return;
      default:
        break;
    }
  }

  lv_label_set_text_fmt(hint_label_, ">> %s", kSubmenus[active_section_].items[selected_index_]);
  if (audio_) audio_->Play(drivers::SoundId::Success);

  // Quick scale-ish feedback via opacity blink on selected row
  if (menu_rows_[selected_index_ - menu_scroll_]) {
    lv_anim_t blink;
    lv_anim_init(&blink);
    lv_anim_set_var(&blink, menu_rows_[selected_index_ - menu_scroll_]);
    lv_anim_set_values(&blink, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_time(&blink, 180);
    lv_anim_set_exec_cb(&blink, AnimOpa);
    lv_anim_start(&blink);
  }

  lv_timer_t* timer = lv_timer_create(
      [](lv_timer_t* t) {
        auto* label = static_cast<lv_obj_t*>(lv_timer_get_user_data(t));
        lv_label_set_text(label, ";/.  Enter  Del");
        lv_timer_delete(t);
      },
      900, hint_label_);
  lv_timer_set_repeat_count(timer, 1);
}

void UiManager::OpenMqttClient() {
  in_mqtt_screen_ = true;
  mqtt_edit_mode_ = false;
  mqtt_selected_ = 0;
  mqtt_scroll_ = 0;
  mqtt_edit_buf_[0] = '\0';
  mqtt_edit_len_ = 0;
  if (mqtt_ != nullptr) {
    const auto tel = mqtt_->GetTelemetry();
    last_mqtt_state_ = tel.state;
    last_mqtt_has_rx_ = tel.has_rx;
  }
  LayoutListRows(kRowStep);
  if (keyboard_) keyboard_->SetTextCapture(false);
  RefreshMqttScreen(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseMqttClient() {
  CancelMqttEdit();
  in_mqtt_screen_ = false;
  LayoutListRows(kRowStep);
  if (keyboard_) keyboard_->SetTextCapture(false);
  RefreshMenuList(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::BeginMqttEdit() {
  if (mqtt_ == nullptr) return;
  if (mqtt_selected_ < 1 || mqtt_selected_ > 5) return;

  auto& cfg = mqtt_->Config();
  mqtt_edit_buf_[0] = '\0';
  mqtt_edit_len_ = 0;
  const char* src = nullptr;
  size_t max_len = sizeof(mqtt_edit_buf_) - 1;
  switch (mqtt_selected_) {
    case 1:
      src = cfg.host;
      max_len = sizeof(cfg.host) - 1;
      break;
    case 2: {
      snprintf(mqtt_edit_buf_, sizeof(mqtt_edit_buf_), "%u", cfg.port);
      mqtt_edit_len_ = static_cast<uint8_t>(strlen(mqtt_edit_buf_));
      mqtt_edit_mode_ = true;
      if (keyboard_) keyboard_->SetTextCapture(true);
      RefreshMqttScreen(false);
      return;
    }
    case 3:
      src = cfg.sub_topic;
      max_len = sizeof(cfg.sub_topic) - 1;
      break;
    case 4:
      src = cfg.pub_topic;
      max_len = sizeof(cfg.pub_topic) - 1;
      break;
    case 5:
      src = cfg.message;
      max_len = sizeof(cfg.message) - 1;
      break;
    default:
      return;
  }
  strncpy(mqtt_edit_buf_, src, max_len);
  mqtt_edit_buf_[max_len] = '\0';
  mqtt_edit_len_ = static_cast<uint8_t>(strlen(mqtt_edit_buf_));
  mqtt_edit_mode_ = true;
  if (keyboard_) keyboard_->SetTextCapture(true);
  RefreshMqttScreen(false);
}

void UiManager::CommitMqttEdit() {
  if (mqtt_ == nullptr || !mqtt_edit_mode_) return;
  auto& cfg = mqtt_->Config();
  switch (mqtt_selected_) {
    case 1:
      strncpy(cfg.host, mqtt_edit_buf_, sizeof(cfg.host) - 1);
      cfg.host[sizeof(cfg.host) - 1] = '\0';
      break;
    case 2: {
      const long port = atol(mqtt_edit_buf_);
      if (port > 0 && port <= 65535) {
        cfg.port = static_cast<uint16_t>(port);
      }
      break;
    }
    case 3:
      strncpy(cfg.sub_topic, mqtt_edit_buf_, sizeof(cfg.sub_topic) - 1);
      cfg.sub_topic[sizeof(cfg.sub_topic) - 1] = '\0';
      break;
    case 4:
      strncpy(cfg.pub_topic, mqtt_edit_buf_, sizeof(cfg.pub_topic) - 1);
      cfg.pub_topic[sizeof(cfg.pub_topic) - 1] = '\0';
      break;
    case 5:
      strncpy(cfg.message, mqtt_edit_buf_, sizeof(cfg.message) - 1);
      cfg.message[sizeof(cfg.message) - 1] = '\0';
      break;
    default:
      break;
  }
  mqtt_edit_mode_ = false;
  mqtt_edit_buf_[0] = '\0';
  mqtt_edit_len_ = 0;
  if (keyboard_) keyboard_->SetTextCapture(false);
  RefreshMqttScreen(false);
}

void UiManager::CancelMqttEdit() {
  mqtt_edit_mode_ = false;
  mqtt_edit_buf_[0] = '\0';
  mqtt_edit_len_ = 0;
  if (keyboard_) keyboard_->SetTextCapture(false);
}

void UiManager::RefreshMqttScreen(bool animate_in) {
  LayoutListRows(kRowStep);

  constexpr int kTotal = 6;
  constexpr int kVisible = 4;

  char lines[kTotal][40];
  const char* status = "выкл";
  if (mqtt_ != nullptr) {
    const auto tel = mqtt_->GetTelemetry();
    switch (tel.state) {
      case modules::MqttState::Connecting:
        status = "...";
        break;
      case modules::MqttState::Connected:
        status = "вкл";
        break;
      case modules::MqttState::Error:
        status = "ошиб";
        break;
      default:
        status = "выкл";
        break;
    }
  }

  const bool wifi_ok = system_status_.wifi.connected;
  snprintf(lines[0], sizeof(lines[0]), "%s  %s",
           mqtt_ != nullptr && mqtt_->IsConnected() ? "Отключить" : "Подключить", status);

  if (mqtt_edit_mode_ && mqtt_selected_ == 1) {
    TruncateText(lines[1], sizeof(lines[1]), mqtt_edit_buf_, 28);
  } else if (mqtt_ != nullptr) {
    char host[32];
    TruncateText(host, sizeof(host), mqtt_->Config().host, 24);
    snprintf(lines[1], sizeof(lines[1]), "Хост %s", host);
  } else {
    snprintf(lines[1], sizeof(lines[1]), "Хост -");
  }

  if (mqtt_edit_mode_ && mqtt_selected_ == 2) {
    snprintf(lines[2], sizeof(lines[2]), "Порт %s", mqtt_edit_buf_);
  } else if (mqtt_ != nullptr) {
    snprintf(lines[2], sizeof(lines[2]), "Порт %u", mqtt_->Config().port);
  } else {
    snprintf(lines[2], sizeof(lines[2]), "Порт -");
  }

  if (mqtt_edit_mode_ && mqtt_selected_ == 3) {
    TruncateText(lines[3], sizeof(lines[3]), mqtt_edit_buf_, 28);
  } else if (mqtt_ != nullptr) {
    char sub[32];
    TruncateText(sub, sizeof(sub), mqtt_->Config().sub_topic, 24);
    snprintf(lines[3], sizeof(lines[3]), "Подп %s", sub);
  } else {
    snprintf(lines[3], sizeof(lines[3]), "Подп -");
  }

  if (mqtt_edit_mode_ && mqtt_selected_ == 4) {
    TruncateText(lines[4], sizeof(lines[4]), mqtt_edit_buf_, 28);
  } else if (mqtt_ != nullptr) {
    char pub[32];
    TruncateText(pub, sizeof(pub), mqtt_->Config().pub_topic, 24);
    snprintf(lines[4], sizeof(lines[4]), "Публ %s", pub);
  } else {
    snprintf(lines[4], sizeof(lines[4]), "Публ -");
  }

  if (mqtt_edit_mode_ && mqtt_selected_ == 5) {
    TruncateText(lines[5], sizeof(lines[5]), mqtt_edit_buf_, 28);
  } else if (mqtt_ != nullptr) {
    char msg[32];
    TruncateText(msg, sizeof(msg), mqtt_->Config().message, 24);
    snprintf(lines[5], sizeof(lines[5]), "Сооб %s", msg);
  } else {
    snprintf(lines[5], sizeof(lines[5]), "Сооб -");
  }

  if (mqtt_selected_ < 0) mqtt_selected_ = 0;
  if (mqtt_selected_ >= kTotal) mqtt_selected_ = kTotal - 1;
  if (mqtt_selected_ < mqtt_scroll_) mqtt_scroll_ = mqtt_selected_;
  if (mqtt_selected_ >= mqtt_scroll_ + kVisible) mqtt_scroll_ = mqtt_selected_ - kVisible + 1;

  for (int row = 0; row < 6; ++row) {
    const int idx = mqtt_scroll_ + row;
    if (row >= kVisible || idx >= kTotal) {
      lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(menu_texts_[row], lines[idx]);
    lv_obj_set_style_text_color(menu_texts_[row],
                                lv_color_hex(idx == mqtt_selected_ ? 0xEAFBFF : 0x9EB0C4), 0);
  }

  const int cursor_row = mqtt_selected_ - mqtt_scroll_;
  if (selection_cursor_) {
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) {
      lv_obj_set_y(selection_cursor_, cursor_row * kRowStep);
      lv_obj_set_style_opa(selection_cursor_, LV_OPA_COVER, 0);
    } else {
      AnimateSelectionCursor(cursor_row);
    }
  }

  if (mqtt_ != nullptr && mqtt_->GetTelemetry().has_rx) {
    char rx[48];
    TruncateText(rx, sizeof(rx), mqtt_->GetTelemetry().last_rx_payload, 36);
    lv_label_set_text_fmt(hint_label_, "Вх: %s", rx);
  } else if (mqtt_edit_mode_) {
    lv_label_set_text(hint_label_, "Enter OK   Del стереть   ` назад");
  } else if (!wifi_ok) {
    lv_label_set_text(hint_label_, "Нужен WiFi");
  } else {
    lv_label_set_text(hint_label_, ";/. Enter  R публ  Del");
  }

  if (animate_in) {
    AnimateItemsIn(kVisible, true);
  }
}

void UiManager::HandleMqttInput(const UiInputEvent& event) {
  using A = drivers::InputAction;

  if (mqtt_edit_mode_) {
    switch (event.action) {
      case A::Char:
        if (mqtt_edit_len_ < sizeof(mqtt_edit_buf_) - 1 && event.ch != 0) {
          if (mqtt_selected_ == 2 && (event.ch < '0' || event.ch > '9')) break;
          mqtt_edit_buf_[mqtt_edit_len_++] = event.ch;
          mqtt_edit_buf_[mqtt_edit_len_] = '\0';
          if (audio_) audio_->Play(drivers::SoundId::KeyClick);
          RefreshMqttScreen(false);
        }
        break;
      case A::DeleteChar:
        if (mqtt_edit_len_ > 0) {
          mqtt_edit_buf_[--mqtt_edit_len_] = '\0';
          if (audio_) audio_->Play(drivers::SoundId::KeyClick);
          RefreshMqttScreen(false);
        } else {
          CancelMqttEdit();
          RefreshMqttScreen(false);
          if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        }
        break;
      case A::Back:
        CancelMqttEdit();
        RefreshMqttScreen(false);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        break;
      case A::Select:
        CommitMqttEdit();
        if (audio_) audio_->Play(drivers::SoundId::Success);
        break;
      default:
        break;
    }
    return;
  }

  switch (event.action) {
    case A::Up:
      mqtt_selected_ = (mqtt_selected_ - 1 + 6) % 6;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshMqttScreen(false);
      break;
    case A::Down:
      mqtt_selected_ = (mqtt_selected_ + 1) % 6;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshMqttScreen(false);
      break;
    case A::Back:
      CloseMqttClient();
      break;
    case A::Select:
      if (mqtt_ == nullptr) {
        if (audio_) audio_->Play(drivers::SoundId::Error);
        break;
      }
      if (mqtt_selected_ == 0) {
        if (mqtt_->IsConnected()) {
          mqtt_->Disconnect();
          if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        } else {
          if (!system_status_.wifi.connected) {
            if (audio_) audio_->Play(drivers::SoundId::Error);
          } else if (mqtt_->Connect()) {
            if (audio_) audio_->Play(drivers::SoundId::Success);
          } else if (audio_) {
            audio_->Play(drivers::SoundId::Error);
          }
        }
        last_mqtt_state_ = mqtt_->GetTelemetry().state;
        RefreshMqttScreen(false);
      } else if (mqtt_selected_ >= 1 && mqtt_selected_ <= 5) {
        BeginMqttEdit();
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      }
      break;
    case A::Rescan:
      if (mqtt_ != nullptr && mqtt_->Publish()) {
        if (audio_) audio_->Play(drivers::SoundId::Success);
      } else if (audio_) {
        audio_->Play(drivers::SoundId::Error);
      }
      break;
    default:
      break;
  }
}

void UiManager::OpenNetTool(NetToolId id) {
  net_tool_ = id;
  net_edit_mode_ = false;
  net_selected_ = 0;
  net_scroll_ = 0;
  net_edit_buf_[0] = '\0';
  net_edit_len_ = 0;
  LayoutListRows(kRowStep);
  if (keyboard_) keyboard_->SetTextCapture(false);
  RefreshNetToolScreen(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseNetTool() {
  CancelNetEdit();
  if (net_tool_ == NetToolId::Websocket && websocket_ != nullptr) {
    // keep connection alive intentionally
  }
  net_tool_ = NetToolId::None;
  LayoutListRows(kRowStep);
  if (keyboard_) keyboard_->SetTextCapture(false);
  RefreshMenuList(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

int UiManager::NetToolRowCount() const {
  switch (net_tool_) {
    case NetToolId::Websocket:
      return 5;
    case NetToolId::Http:
      return 5;
    case NetToolId::Tcp:
      return 5;
    case NetToolId::Ping:
      return 4;
    case NetToolId::Info:
      return 6;
    default:
      return 0;
  }
}

void UiManager::FillNetToolLines(char lines[][40], int count) {
  for (int i = 0; i < count; ++i) lines[i][0] = '\0';

  switch (net_tool_) {
    case NetToolId::Websocket: {
      if (websocket_ == nullptr) break;
      auto& cfg = websocket_->Config();
      const auto tel = websocket_->GetTelemetry();
      const char* st = "выкл";
      if (tel.state == modules::WsState::Connecting) st = "...";
      else if (tel.state == modules::WsState::Connected) st = "вкл";
      else if (tel.state == modules::WsState::Error) st = "ошиб";
      snprintf(lines[0], 40, "%s  %s", websocket_->IsConnected() ? "Отключить" : "Подключить", st);
      if (net_edit_mode_ && net_selected_ == 1) TruncateText(lines[1], 40, net_edit_buf_, 28);
      else {
        char h[28];
        TruncateText(h, sizeof(h), cfg.host, 24);
        snprintf(lines[1], 40, "Хост %s", h);
      }
      if (net_edit_mode_ && net_selected_ == 2) snprintf(lines[2], 40, "Порт %s", net_edit_buf_);
      else snprintf(lines[2], 40, "Порт %u", cfg.port);
      if (net_edit_mode_ && net_selected_ == 3) TruncateText(lines[3], 40, net_edit_buf_, 28);
      else {
        char p[28];
        TruncateText(p, sizeof(p), cfg.path, 24);
        snprintf(lines[3], 40, "Путь %s", p);
      }
      if (net_edit_mode_ && net_selected_ == 4) TruncateText(lines[4], 40, net_edit_buf_, 28);
      else {
        char m[28];
        TruncateText(m, sizeof(m), cfg.message, 24);
        snprintf(lines[4], 40, "Сооб %s", m);
      }
      break;
    }
    case NetToolId::Http: {
      if (http_ == nullptr) break;
      auto& cfg = http_->Config();
      const auto tel = http_->GetTelemetry();
      const char* st = "—";
      if (tel.state == modules::HttpState::Busy) st = "...";
      else if (tel.state == modules::HttpState::Ok) st = "ок";
      else if (tel.state == modules::HttpState::Error) st = "ошиб";
      snprintf(lines[0], 40, "Запрос  %s", st);
      snprintf(lines[1], 40, "Метод %s", cfg.method == modules::HttpMethod::Post ? "POST" : "GET");
      if (net_edit_mode_ && net_selected_ == 2) TruncateText(lines[2], 40, net_edit_buf_, 32);
      else {
        char u[36];
        TruncateText(u, sizeof(u), cfg.url, 30);
        snprintf(lines[2], 40, "Адрес %s", u);
      }
      if (net_edit_mode_ && net_selected_ == 3) TruncateText(lines[3], 40, net_edit_buf_, 28);
      else {
        char b[28];
        TruncateText(b, sizeof(b), cfg.body, 24);
        snprintf(lines[3], 40, "Тело %s", b);
      }
      snprintf(lines[4], 40, "Код  %d", tel.code);
      break;
    }
    case NetToolId::Tcp: {
      if (tcp_ == nullptr) break;
      auto& cfg = tcp_->Config();
      const auto tel = tcp_->GetTelemetry();
      const char* st = "выкл";
      if (tel.state == modules::TcpState::Connecting) st = "...";
      else if (tel.state == modules::TcpState::Connected) st = "вкл";
      else if (tel.state == modules::TcpState::Error) st = "ошиб";
      snprintf(lines[0], 40, "%s  %s", tcp_->IsConnected() ? "Отключить" : "Подключить", st);
      if (net_edit_mode_ && net_selected_ == 1) TruncateText(lines[1], 40, net_edit_buf_, 28);
      else {
        char h[28];
        TruncateText(h, sizeof(h), cfg.host, 24);
        snprintf(lines[1], 40, "Хост %s", h);
      }
      if (net_edit_mode_ && net_selected_ == 2) snprintf(lines[2], 40, "Порт %s", net_edit_buf_);
      else snprintf(lines[2], 40, "Порт %u", cfg.port);
      if (net_edit_mode_ && net_selected_ == 3) TruncateText(lines[3], 40, net_edit_buf_, 28);
      else {
        char m[28];
        TruncateText(m, sizeof(m), cfg.message, 24);
        snprintf(lines[3], 40, "Сооб %s", m);
      }
      snprintf(lines[4], 40, "Отправ.  R");
      break;
    }
    case NetToolId::Ping: {
      if (ping_ == nullptr) break;
      auto& cfg = ping_->Config();
      const auto tel = ping_->GetTelemetry();
      if (net_edit_mode_ && net_selected_ == 0) TruncateText(lines[0], 40, net_edit_buf_, 28);
      else {
        char h[28];
        TruncateText(h, sizeof(h), cfg.host, 24);
        snprintf(lines[0], 40, "Хост %s", h);
      }
      snprintf(lines[1], 40, "DNS  %s", tel.resolved ? tel.ip : "—");
      if (tel.state == modules::PingState::Busy) snprintf(lines[2], 40, "Пинг ...");
      else if (tel.state == modules::PingState::Ok)
        snprintf(lines[2], 40, "Пинг %.0f мс", tel.avg_ms);
      else if (tel.state == modules::PingState::Error)
        snprintf(lines[2], 40, "Пинг ошибка");
      else
        snprintf(lines[2], 40, "Пинг  Enter");
      snprintf(lines[3], 40, "DNS   Enter");
      break;
    }
    case NetToolId::Info: {
      const bool ok = system_status_.wifi.connected;
      char ip[20] = "—", gw[20] = "—", dns[20] = "—", mac[20] = "—";
      if (ok) {
        const IPAddress a = WiFi.localIP();
        const IPAddress g = WiFi.gatewayIP();
        const IPAddress d = WiFi.dnsIP();
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
        snprintf(gw, sizeof(gw), "%u.%u.%u.%u", g[0], g[1], g[2], g[3]);
        snprintf(dns, sizeof(dns), "%u.%u.%u.%u", d[0], d[1], d[2], d[3]);
        strncpy(mac, WiFi.macAddress().c_str(), sizeof(mac) - 1);
      }
      char ssid[28];
      TruncateText(ssid, sizeof(ssid),
                   ok ? system_status_.wifi.connected_ssid : "нет сети", 20);
      snprintf(lines[0], 40, "Сеть %s", ssid);
      snprintf(lines[1], 40, "IP   %s", ip);
      snprintf(lines[2], 40, "Шлюз %s", gw);
      snprintf(lines[3], 40, "DNS  %s", dns);
      snprintf(lines[4], 40, "Сигн %ld  %s", static_cast<long>(system_status_.wifi.link_rssi),
               mac);
      snprintf(lines[5], 40, "Время %s", ntp_status_);
      break;
    }
    default:
      break;
  }
}

void UiManager::RefreshNetToolScreen(bool animate_in) {
  LayoutListRows(kRowStep);
  const int total = NetToolRowCount();
  char lines[8][40];
  FillNetToolLines(lines, total);

  if (net_selected_ < 0) net_selected_ = 0;
  if (total > 0 && net_selected_ >= total) net_selected_ = total - 1;
  if (net_selected_ < net_scroll_) net_scroll_ = net_selected_;
  if (net_selected_ >= net_scroll_ + kVisibleRows) net_scroll_ = net_selected_ - kVisibleRows + 1;

  for (int row = 0; row < 6; ++row) {
    const int idx = net_scroll_ + row;
    if (row >= kVisibleRows || idx >= total) {
      lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(menu_texts_[row], lines[idx]);
    lv_obj_set_style_text_color(menu_texts_[row],
                                lv_color_hex(idx == net_selected_ ? 0xEAFBFF : 0x9EB0C4), 0);
  }

  const int cursor_row = net_selected_ - net_scroll_;
  if (selection_cursor_) {
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) {
      lv_obj_set_y(selection_cursor_, cursor_row * kRowStep);
      lv_obj_set_style_opa(selection_cursor_, LV_OPA_COVER, 0);
    } else {
      AnimateSelectionCursor(cursor_row);
    }
  }

  const bool wifi_ok = system_status_.wifi.connected;
  if (net_edit_mode_) {
    lv_label_set_text(hint_label_, "Enter OK   Del   ` назад");
  } else if (net_tool_ == NetToolId::Websocket && websocket_ != nullptr &&
             websocket_->GetTelemetry().has_rx) {
    char rx[40];
    TruncateText(rx, sizeof(rx), websocket_->GetTelemetry().last_rx, 32);
    lv_label_set_text_fmt(hint_label_, "Вх: %s", rx);
  } else if (net_tool_ == NetToolId::Tcp && tcp_ != nullptr && tcp_->GetTelemetry().has_rx) {
    char rx[40];
    TruncateText(rx, sizeof(rx), tcp_->GetTelemetry().last_rx, 32);
    lv_label_set_text_fmt(hint_label_, "Вх: %s", rx);
  } else if (net_tool_ == NetToolId::Http && http_ != nullptr &&
             http_->GetTelemetry().preview[0] != '\0') {
    char rx[40];
    TruncateText(rx, sizeof(rx), http_->GetTelemetry().preview, 32);
    lv_label_set_text_fmt(hint_label_, "%s", rx);
  } else if (!wifi_ok && net_tool_ != NetToolId::Info) {
    lv_label_set_text(hint_label_, "Нужен WiFi");
  } else if (net_tool_ == NetToolId::Websocket || net_tool_ == NetToolId::Tcp) {
    lv_label_set_text(hint_label_, ";/. Enter  R отпр  Del");
  } else if (net_tool_ == NetToolId::Http) {
    lv_label_set_text(hint_label_, ";/. Enter  R запрос  Del");
  } else {
    lv_label_set_text(hint_label_, ";/.  Enter  Del");
  }

  if (animate_in) AnimateItemsIn(kVisibleRows < total ? kVisibleRows : total, true);
}

void UiManager::BeginNetEdit() {
  net_edit_buf_[0] = '\0';
  net_edit_len_ = 0;
  const char* src = nullptr;
  size_t max_len = sizeof(net_edit_buf_) - 1;
  bool numeric = false;

  switch (net_tool_) {
    case NetToolId::Websocket:
      if (websocket_ == nullptr) return;
      if (net_selected_ == 1) {
        src = websocket_->Config().host;
        max_len = sizeof(websocket_->Config().host) - 1;
      } else if (net_selected_ == 2) {
        snprintf(net_edit_buf_, sizeof(net_edit_buf_), "%u", websocket_->Config().port);
        numeric = true;
      } else if (net_selected_ == 3) {
        src = websocket_->Config().path;
        max_len = sizeof(websocket_->Config().path) - 1;
      } else if (net_selected_ == 4) {
        src = websocket_->Config().message;
        max_len = sizeof(websocket_->Config().message) - 1;
      } else
        return;
      break;
    case NetToolId::Http:
      if (http_ == nullptr) return;
      if (net_selected_ == 2) {
        src = http_->Config().url;
        max_len = sizeof(http_->Config().url) - 1;
      } else if (net_selected_ == 3) {
        src = http_->Config().body;
        max_len = sizeof(http_->Config().body) - 1;
      } else
        return;
      break;
    case NetToolId::Tcp:
      if (tcp_ == nullptr) return;
      if (net_selected_ == 1) {
        src = tcp_->Config().host;
        max_len = sizeof(tcp_->Config().host) - 1;
      } else if (net_selected_ == 2) {
        snprintf(net_edit_buf_, sizeof(net_edit_buf_), "%u", tcp_->Config().port);
        numeric = true;
      } else if (net_selected_ == 3) {
        src = tcp_->Config().message;
        max_len = sizeof(tcp_->Config().message) - 1;
      } else
        return;
      break;
    case NetToolId::Ping:
      if (ping_ == nullptr || net_selected_ != 0) return;
      src = ping_->Config().host;
      max_len = sizeof(ping_->Config().host) - 1;
      break;
    default:
      return;
  }

  if (!numeric && src != nullptr) {
    strncpy(net_edit_buf_, src, max_len);
    net_edit_buf_[max_len] = '\0';
  }
  net_edit_len_ = static_cast<uint8_t>(strlen(net_edit_buf_));
  net_edit_mode_ = true;
  if (keyboard_) keyboard_->SetTextCapture(true);
  RefreshNetToolScreen(false);
}

void UiManager::CommitNetEdit() {
  if (!net_edit_mode_) return;
  switch (net_tool_) {
    case NetToolId::Websocket:
      if (websocket_ != nullptr) {
        auto& cfg = websocket_->Config();
        if (net_selected_ == 1) {
          strncpy(cfg.host, net_edit_buf_, sizeof(cfg.host) - 1);
          cfg.host[sizeof(cfg.host) - 1] = '\0';
        } else if (net_selected_ == 2) {
          const long p = atol(net_edit_buf_);
          if (p > 0 && p <= 65535) cfg.port = static_cast<uint16_t>(p);
        } else if (net_selected_ == 3) {
          strncpy(cfg.path, net_edit_buf_, sizeof(cfg.path) - 1);
          cfg.path[sizeof(cfg.path) - 1] = '\0';
        } else if (net_selected_ == 4) {
          strncpy(cfg.message, net_edit_buf_, sizeof(cfg.message) - 1);
          cfg.message[sizeof(cfg.message) - 1] = '\0';
        }
      }
      break;
    case NetToolId::Http:
      if (http_ != nullptr) {
        auto& cfg = http_->Config();
        if (net_selected_ == 2) {
          strncpy(cfg.url, net_edit_buf_, sizeof(cfg.url) - 1);
          cfg.url[sizeof(cfg.url) - 1] = '\0';
        } else if (net_selected_ == 3) {
          strncpy(cfg.body, net_edit_buf_, sizeof(cfg.body) - 1);
          cfg.body[sizeof(cfg.body) - 1] = '\0';
        }
      }
      break;
    case NetToolId::Tcp:
      if (tcp_ != nullptr) {
        auto& cfg = tcp_->Config();
        if (net_selected_ == 1) {
          strncpy(cfg.host, net_edit_buf_, sizeof(cfg.host) - 1);
          cfg.host[sizeof(cfg.host) - 1] = '\0';
        } else if (net_selected_ == 2) {
          const long p = atol(net_edit_buf_);
          if (p > 0 && p <= 65535) cfg.port = static_cast<uint16_t>(p);
        } else if (net_selected_ == 3) {
          strncpy(cfg.message, net_edit_buf_, sizeof(cfg.message) - 1);
          cfg.message[sizeof(cfg.message) - 1] = '\0';
        }
      }
      break;
    case NetToolId::Ping:
      if (ping_ != nullptr && net_selected_ == 0) {
        strncpy(ping_->Config().host, net_edit_buf_, sizeof(ping_->Config().host) - 1);
        ping_->Config().host[sizeof(ping_->Config().host) - 1] = '\0';
      }
      break;
    default:
      break;
  }
  CancelNetEdit();
  RefreshNetToolScreen(false);
}

void UiManager::CancelNetEdit() {
  net_edit_mode_ = false;
  net_edit_buf_[0] = '\0';
  net_edit_len_ = 0;
  if (keyboard_) keyboard_->SetTextCapture(false);
}

void UiManager::RunNetAction() {
  bool ok = false;
  switch (net_tool_) {
    case NetToolId::Websocket:
      if (websocket_ != nullptr) ok = websocket_->Send();
      break;
    case NetToolId::Http:
      if (http_ != nullptr) ok = http_->Request();
      break;
    case NetToolId::Tcp:
      if (tcp_ != nullptr) ok = tcp_->Send();
      break;
    case NetToolId::Ping:
      if (ping_ != nullptr) {
        if (net_selected_ == 3) ok = ping_->Resolve();
        else ok = ping_->Run();
      }
      break;
    case NetToolId::Info:
      if (net_selected_ == 5) {
        if (!system_status_.wifi.connected) {
          strncpy(ntp_status_, "нет WiFi", sizeof(ntp_status_) - 1);
          ok = false;
        } else {
          configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
          struct tm tinfo;
          if (getLocalTime(&tinfo, 5000)) {
            snprintf(ntp_status_, sizeof(ntp_status_), "%02d:%02d:%02d", tinfo.tm_hour,
                     tinfo.tm_min, tinfo.tm_sec);
            ok = true;
          } else {
            strncpy(ntp_status_, "ошибка", sizeof(ntp_status_) - 1);
            ok = false;
          }
        }
      }
      break;
    default:
      break;
  }
  if (audio_) audio_->Play(ok ? drivers::SoundId::Success : drivers::SoundId::Error);
  RefreshNetToolScreen(false);
}

void UiManager::HandleNetToolInput(const UiInputEvent& event) {
  using A = drivers::InputAction;
  const int total = NetToolRowCount();

  if (net_edit_mode_) {
    const bool port_field =
        (net_tool_ == NetToolId::Websocket && net_selected_ == 2) ||
        (net_tool_ == NetToolId::Tcp && net_selected_ == 2);
    switch (event.action) {
      case A::Char:
        if (net_edit_len_ < sizeof(net_edit_buf_) - 1 && event.ch != 0) {
          if (port_field && (event.ch < '0' || event.ch > '9')) break;
          net_edit_buf_[net_edit_len_++] = event.ch;
          net_edit_buf_[net_edit_len_] = '\0';
          if (audio_) audio_->Play(drivers::SoundId::KeyClick);
          RefreshNetToolScreen(false);
        }
        break;
      case A::DeleteChar:
        if (net_edit_len_ > 0) {
          net_edit_buf_[--net_edit_len_] = '\0';
          if (audio_) audio_->Play(drivers::SoundId::KeyClick);
          RefreshNetToolScreen(false);
        } else {
          CancelNetEdit();
          RefreshNetToolScreen(false);
        }
        break;
      case A::Back:
        CancelNetEdit();
        RefreshNetToolScreen(false);
        break;
      case A::Select:
        CommitNetEdit();
        if (audio_) audio_->Play(drivers::SoundId::Success);
        break;
      default:
        break;
    }
    return;
  }

  switch (event.action) {
    case A::Up:
      if (total == 0) break;
      net_selected_ = (net_selected_ - 1 + total) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshNetToolScreen(false);
      break;
    case A::Down:
      if (total == 0) break;
      net_selected_ = (net_selected_ + 1) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshNetToolScreen(false);
      break;
    case A::Back:
      CloseNetTool();
      break;
    case A::Rescan:
      RunNetAction();
      break;
    case A::Select: {
      bool handled = false;
      if (net_tool_ == NetToolId::Websocket && websocket_ != nullptr && net_selected_ == 0) {
        if (websocket_->IsConnected()) {
          websocket_->Disconnect();
          if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        } else if (!system_status_.wifi.connected) {
          if (audio_) audio_->Play(drivers::SoundId::Error);
        } else if (websocket_->Connect()) {
          if (audio_) audio_->Play(drivers::SoundId::Success);
        } else if (audio_) {
          audio_->Play(drivers::SoundId::Error);
        }
        handled = true;
      } else if (net_tool_ == NetToolId::Tcp && tcp_ != nullptr && net_selected_ == 0) {
        if (tcp_->IsConnected()) {
          tcp_->Disconnect();
          if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        } else if (!system_status_.wifi.connected) {
          if (audio_) audio_->Play(drivers::SoundId::Error);
        } else if (tcp_->Connect()) {
          if (audio_) audio_->Play(drivers::SoundId::Success);
        } else if (audio_) {
          audio_->Play(drivers::SoundId::Error);
        }
        handled = true;
      } else if (net_tool_ == NetToolId::Http && http_ != nullptr && net_selected_ == 0) {
        RunNetAction();
        handled = true;
      } else if (net_tool_ == NetToolId::Http && http_ != nullptr && net_selected_ == 1) {
        http_->Config().method = http_->Config().method == modules::HttpMethod::Get
                                     ? modules::HttpMethod::Post
                                     : modules::HttpMethod::Get;
        if (audio_) audio_->Play(drivers::SoundId::KeyClick);
        handled = true;
      } else if (net_tool_ == NetToolId::Ping && (net_selected_ == 2 || net_selected_ == 3)) {
        RunNetAction();
        handled = true;
      } else if (net_tool_ == NetToolId::Info && net_selected_ == 5) {
        RunNetAction();
        handled = true;
      } else if (net_tool_ == NetToolId::Ping && net_selected_ == 0) {
        BeginNetEdit();
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        handled = true;
      } else if ((net_tool_ == NetToolId::Websocket && net_selected_ >= 1 && net_selected_ <= 4) ||
                 (net_tool_ == NetToolId::Http && (net_selected_ == 2 || net_selected_ == 3)) ||
                 (net_tool_ == NetToolId::Tcp && net_selected_ >= 1 && net_selected_ <= 3)) {
        BeginNetEdit();
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        handled = true;
      } else if (net_tool_ == NetToolId::Tcp && net_selected_ == 4) {
        RunNetAction();
        handled = true;
      }
      if (handled) RefreshNetToolScreen(false);
      break;
    }
    default:
      break;
  }
}

void UiManager::FormatBytes(char* dst, size_t n, uint64_t bytes) {
  if (bytes >= (1024ULL * 1024ULL * 1024ULL)) {
    snprintf(dst, n, "%.1fG", bytes / (1024.0 * 1024.0 * 1024.0));
  } else if (bytes >= (1024ULL * 1024ULL)) {
    snprintf(dst, n, "%.1fM", bytes / (1024.0 * 1024.0));
  } else if (bytes >= 1024ULL) {
    snprintf(dst, n, "%.0fK", bytes / 1024.0);
  } else {
    snprintf(dst, n, "%lluB", static_cast<unsigned long long>(bytes));
  }
}

void UiManager::EnsureRadioSpecUi() {
  if (radio_spec_panel_ != nullptr) return;
  radio_spec_panel_ = lv_obj_create(menu_screen_);
  lv_obj_remove_style_all(radio_spec_panel_);
  lv_obj_set_size(radio_spec_panel_, 224, 84);
  lv_obj_align(radio_spec_panel_, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_set_style_bg_color(radio_spec_panel_, lv_color_hex(0x0B1220), 0);
  lv_obj_set_style_bg_opa(radio_spec_panel_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(radio_spec_panel_, 6, 0);
  lv_obj_set_style_border_width(radio_spec_panel_, 1, 0);
  lv_obj_set_style_border_color(radio_spec_panel_, lv_color_hex(kPrimaryHex), 0);
  lv_obj_set_style_border_opa(radio_spec_panel_, LV_OPA_40, 0);
  lv_obj_add_flag(radio_spec_panel_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(radio_spec_panel_, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int kBars = 32;
  constexpr int32_t kBarW = 5;
  constexpr int32_t kGap = 2;
  for (int i = 0; i < kBars; ++i) {
    radio_spec_bars_[i] = lv_obj_create(radio_spec_panel_);
    lv_obj_remove_style_all(radio_spec_bars_[i]);
    lv_obj_set_size(radio_spec_bars_[i], kBarW, 4);
    lv_obj_set_style_bg_color(radio_spec_bars_[i], lv_color_hex(kPrimaryHex), 0);
    lv_obj_set_style_bg_opa(radio_spec_bars_[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(radio_spec_bars_[i], 1, 0);
    lv_obj_align(radio_spec_bars_[i], LV_ALIGN_BOTTOM_LEFT, 6 + i * (kBarW + kGap), -16);
  }

  radio_spec_label_ = lv_label_create(radio_spec_panel_);
  lv_obj_set_style_text_font(radio_spec_label_, &font_ru_14, 0);
  lv_obj_set_style_text_color(radio_spec_label_, lv_color_hex(0xEAFBFF), 0);
  lv_label_set_text(radio_spec_label_, "");
  lv_obj_align(radio_spec_label_, LV_ALIGN_TOP_LEFT, 6, 4);
}

void UiManager::OpenRadioTool(RadioToolId id) {
  radio_tool_ = id;
  radio_selected_ = 0;
  radio_scroll_ = 0;
  radio_spec_origin_ = 0;
  LayoutListRows(kRowStep);

  if (nrf_ != nullptr) {
    if (id == RadioToolId::Scanner) {
      EnsureRadioSpecUi();
      nrf_->SetScannerActive(true);
      if (menu_container_) lv_obj_add_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);
      if (selection_cursor_) lv_obj_add_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
      for (int i = 0; i < 6; ++i) {
        if (menu_rows_[i]) lv_obj_add_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
      }
      if (radio_spec_panel_) lv_obj_clear_flag(radio_spec_panel_, LV_OBJ_FLAG_HIDDEN);
    } else if (id == RadioToolId::Monitor) {
      nrf_->SetMonitorActive(true);
      if (radio_spec_panel_) lv_obj_add_flag(radio_spec_panel_, LV_OBJ_FLAG_HIDDEN);
      if (menu_container_) lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);
    } else {
      nrf_->SetScannerActive(false);
      nrf_->SetMonitorActive(false);
      if (radio_spec_panel_) lv_obj_add_flag(radio_spec_panel_, LV_OBJ_FLAG_HIDDEN);
      if (menu_container_) lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  RefreshRadioScreen(true);
  RefreshStatusBar();
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseRadioTool() {
  if (nrf_ != nullptr) {
    nrf_->SetScannerActive(false);
    nrf_->SetMonitorActive(false);
  }
  radio_tool_ = RadioToolId::None;
  if (radio_spec_panel_) lv_obj_add_flag(radio_spec_panel_, LV_OBJ_FLAG_HIDDEN);
  if (menu_container_) lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);
  LayoutListRows(kRowStep);
  RefreshMenuList(true);
  RefreshStatusBar();
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::RefreshRadioScreen(bool animate_in) {
  if (radio_tool_ == RadioToolId::None) return;

  if (nrf_ == nullptr || !nrf_->IsPresent()) {
    LayoutListRows(kRowStep);
    if (radio_spec_panel_) lv_obj_add_flag(radio_spec_panel_, LV_OBJ_FLAG_HIDDEN);
    if (menu_container_) lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 6; ++i) {
      if (i == 0) {
        lv_obj_remove_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(menu_texts_[i], "nRF24 не найден");
        lv_obj_set_style_text_color(menu_texts_[i], lv_color_hex(0xFF6B6B), 0);
      } else {
        lv_obj_add_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (selection_cursor_) lv_obj_add_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(hint_label_, "Проверь CE/CSN SPI  Del");
    return;
  }

  const auto tel = nrf_->GetTelemetry();

  if (radio_tool_ == RadioToolId::Scanner) {
    EnsureRadioSpecUi();
    constexpr int kBars = 32;
    constexpr int32_t kMaxH = 52;
    if (radio_spec_origin_ < 0) radio_spec_origin_ = 0;
    if (radio_spec_origin_ > 125 - kBars + 1) radio_spec_origin_ = 125 - kBars + 1;

    for (int i = 0; i < kBars; ++i) {
      const uint8_t ch = static_cast<uint8_t>(radio_spec_origin_ + i);
      const uint8_t act = nrf_->ActivityAt(ch);
      int32_t h = static_cast<int32_t>((act * kMaxH) / 100);
      if (h < 2) h = 2;
      lv_obj_set_height(radio_spec_bars_[i], h);
      const bool hot = (ch == tel.hottest_channel);
      lv_obj_set_style_bg_color(radio_spec_bars_[i],
                                lv_color_hex(hot ? 0x5CFF9A : (act > 40 ? 0xFFB84D : kPrimaryHex)),
                                0);
      lv_obj_align(radio_spec_bars_[i], LV_ALIGN_BOTTOM_LEFT, 6 + i * 7, -16);
    }
    lv_label_set_text_fmt(radio_spec_label_, "кан.%u-%u  пик %u:%u%%",
                          radio_spec_origin_, radio_spec_origin_ + kBars - 1, tel.hottest_channel,
                          tel.activity_percent);
    lv_label_set_text(hint_label_, ";/. окно  Enter пик  Del");
    return;
  }

  LayoutListRows(kRowStep);
  if (radio_spec_panel_) lv_obj_add_flag(radio_spec_panel_, LV_OBJ_FLAG_HIDDEN);
  if (menu_container_) lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);

  char lines[16][40];
  int total = 0;

  if (radio_tool_ == RadioToolId::Monitor) {
    const uint8_t n = nrf_->PacketCount();
    snprintf(lines[0], sizeof(lines[0]), "Кан.%u  пак:%u", tel.current_channel, tel.packets_rx);
    total = 1;
    if (n == 0) {
      snprintf(lines[1], sizeof(lines[1]), "(тишина)");
      total = 2;
    } else {
      for (uint8_t i = 0; i < n && total < 16; ++i) {
        const auto& p = nrf_->PacketAt(i);
        char hex[28] = {0};
        size_t pos = 0;
        const uint8_t show = p.len < 8 ? p.len : 8;
        for (uint8_t b = 0; b < show && pos + 3 < sizeof(hex); ++b) {
          pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X", p.data[b]);
          if (b + 1 < show && pos + 1 < sizeof(hex)) hex[pos++] = ' ';
        }
        snprintf(lines[total], sizeof(lines[total]), "#%u %s", p.len, hex);
        ++total;
      }
    }
    lv_label_set_text(hint_label_, "R очист  ;/.  Del");
  } else {
    // Manager
    static const char* kPa[] = {"MIN", "LOW", "HIGH", "MAX"};
    static const char* kRate[] = {"1M", "2M", "250k"};
    const char* pa = tel.pa_level < 4 ? kPa[tel.pa_level] : "?";
    const char* rate = nrf_->DataRate() < 3 ? kRate[nrf_->DataRate()] : "?";
    snprintf(lines[0], sizeof(lines[0]), "Статус  %s", tel.present ? "OK" : "нет");
    snprintf(lines[1], sizeof(lines[1]), "Канал   %u", tel.current_channel);
    snprintf(lines[2], sizeof(lines[2]), "Мощн.   %s", pa);
    snprintf(lines[3], sizeof(lines[3]), "Скор.   %s", rate);
    snprintf(lines[4], sizeof(lines[4]), "TX тест %s", tel.tx_ok ? "OK" : "--");
    snprintf(lines[5], sizeof(lines[5]), "Пик     %u (%u%%)", tel.hottest_channel,
             tel.activity_percent);
    total = 6;
    lv_label_set_text(hint_label_, ";/.  Enter изм/тест  Del");
  }

  if (radio_selected_ < 0) radio_selected_ = 0;
  if (total > 0 && radio_selected_ >= total) radio_selected_ = total - 1;
  if (radio_selected_ < radio_scroll_) radio_scroll_ = radio_selected_;
  if (radio_selected_ >= radio_scroll_ + kVisibleRows) {
    radio_scroll_ = radio_selected_ - kVisibleRows + 1;
  }

  for (int row = 0; row < 6; ++row) {
    const int idx = radio_scroll_ + row;
    if (row >= kVisibleRows || idx >= total) {
      lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(menu_texts_[row], lines[idx]);
    lv_obj_set_style_text_color(menu_texts_[row],
                                lv_color_hex(idx == radio_selected_ ? 0xEAFBFF : 0x9EB0C4), 0);
  }

  const int cursor_row = radio_selected_ - radio_scroll_;
  if (selection_cursor_) {
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) lv_obj_set_y(selection_cursor_, cursor_row * kRowStep);
    else AnimateSelectionCursor(cursor_row);
  }
  if (animate_in) AnimateItemsIn(kVisibleRows < total ? kVisibleRows : total, true);
}

void UiManager::HandleRadioInput(const UiInputEvent& event) {
  using A = drivers::InputAction;
  if (nrf_ == nullptr || !nrf_->IsPresent()) {
    if (event.action == A::Back) CloseRadioTool();
    return;
  }

  if (radio_tool_ == RadioToolId::Scanner) {
    switch (event.action) {
      case A::Up:
        radio_spec_origin_ -= 8;
        if (radio_spec_origin_ < 0) radio_spec_origin_ = 0;
        if (audio_) audio_->Play(drivers::SoundId::KeyClick);
        RefreshRadioScreen(false);
        break;
      case A::Down:
        radio_spec_origin_ += 8;
        if (radio_spec_origin_ > 94) radio_spec_origin_ = 94;
        if (audio_) audio_->Play(drivers::SoundId::KeyClick);
        RefreshRadioScreen(false);
        break;
      case A::Select: {
        const auto tel = nrf_->GetTelemetry();
        nrf_->SetChannel(tel.hottest_channel);
        if (audio_) audio_->Play(drivers::SoundId::Success);
        RefreshRadioScreen(false);
        break;
      }
      case A::Rescan:
        nrf_->SetScannerActive(true);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        break;
      case A::Back:
        CloseRadioTool();
        break;
      default:
        break;
    }
    return;
  }

  int total = 0;
  if (radio_tool_ == RadioToolId::Monitor) {
    total = 1 + (nrf_->PacketCount() == 0 ? 1 : nrf_->PacketCount());
  } else {
    total = 6;
  }

  switch (event.action) {
    case A::Up:
      radio_selected_ = (radio_selected_ - 1 + total) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshRadioScreen(false);
      break;
    case A::Down:
      radio_selected_ = (radio_selected_ + 1) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshRadioScreen(false);
      break;
    case A::Back:
      CloseRadioTool();
      break;
    case A::Rescan:
      if (radio_tool_ == RadioToolId::Monitor) {
        nrf_->ClearPackets();
        radio_selected_ = 0;
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        RefreshRadioScreen(false);
      }
      break;
    case A::Select:
      if (radio_tool_ == RadioToolId::Manager) {
        switch (radio_selected_) {
          case 1: {
            uint8_t ch = nrf_->GetTelemetry().current_channel;
            ch = static_cast<uint8_t>((ch + 1) % 126);
            nrf_->SetChannel(ch);
            break;
          }
          case 2: {
            uint8_t pa = nrf_->GetTelemetry().pa_level;
            pa = static_cast<uint8_t>((pa + 1) % 4);
            nrf_->SetPower(pa);
            break;
          }
          case 3: {
            uint8_t r = nrf_->DataRate();
            r = static_cast<uint8_t>((r + 1) % 3);
            nrf_->SetDataRate(r);
            break;
          }
          case 4:
            if (nrf_->TxTestPacket()) {
              if (audio_) audio_->Play(drivers::SoundId::Success);
            } else if (audio_) {
              audio_->Play(drivers::SoundId::Error);
            }
            break;
          case 5:
            nrf_->SetChannel(nrf_->GetTelemetry().hottest_channel);
            if (audio_) audio_->Play(drivers::SoundId::Success);
            break;
          default:
            break;
        }
        RefreshRadioScreen(false);
      }
      break;
    default:
      break;
  }
}

void UiManager::OpenHwTool(HwToolId id) {
  hw_tool_ = id;
  hw_selected_ = 0;
  hw_scroll_ = 0;
  last_i2c_scanning_ = false;
  last_i2c_count_ = 0;
  LayoutListRows(kRowStep);
  if (hw_tool_ == HwToolId::I2c && i2c_ != nullptr) {
    i2c_->StartScan(modules::I2cBusId::Internal);
    last_i2c_scanning_ = true;
  }
  RefreshHwToolScreen(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseHwTool() {
  if (in_sensor_graph_) {
    in_sensor_graph_ = false;
    if (graph_panel_) lv_obj_add_flag(graph_panel_, LV_OBJ_FLAG_HIDDEN);
    if (menu_container_) lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);
  }
  hw_tool_ = HwToolId::None;
  LayoutListRows(kRowStep);
  RefreshMenuList(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::RefreshHwToolScreen(bool animate_in) {
  LayoutListRows(kRowStep);
  char lines[12][40];
  int total = 0;

  if (hw_tool_ == HwToolId::Gpio && gpio_ != nullptr) {
    total = gpio_->Count();
    for (int i = 0; i < total; ++i) {
      const auto& p = gpio_->At(static_cast<uint8_t>(i));
      snprintf(lines[i], sizeof(lines[i]), "G%-2u %s %s %s", p.pin, p.level ? "1" : "0",
               p.output ? "вых" : "вх ", p.tag);
    }
  } else if (hw_tool_ == HwToolId::I2c && i2c_ != nullptr) {
    const char* bus = i2c_->Bus() == modules::I2cBusId::Grove ? "Гров" : "Внутр";
    if (i2c_->Scanning()) {
      snprintf(lines[0], sizeof(lines[0]), "Шина %s  %u%%", bus, i2c_->ProgressPercent());
    } else {
      snprintf(lines[0], sizeof(lines[0]), "Шина %s  R скан", bus);
    }
    snprintf(lines[1], sizeof(lines[1]), "Сменить шину");
    const uint8_t n = i2c_->Count();
    total = 2 + n;
    if (i2c_->Scanning() && n == 0) {
      snprintf(lines[2], sizeof(lines[2]), "Сканирование...");
      total = 3;
    } else if (n == 0) {
      snprintf(lines[2], sizeof(lines[2]), "(пусто)");
      total = 3;
    } else {
      for (uint8_t i = 0; i < n; ++i) {
        const auto& d = i2c_->At(i);
        if (d.name[0] != '\0') {
          snprintf(lines[2 + i], sizeof(lines[2 + i]), "0x%02X %s", d.addr, d.name);
        } else {
          snprintf(lines[2 + i], sizeof(lines[2 + i]), "0x%02X", d.addr);
        }
      }
    }
  } else if (hw_tool_ == HwToolId::Sensors && sensors_ != nullptr) {
    const auto t = sensors_->GetTelemetry();
    total = 6;
    if (t.imu_ok) {
      snprintf(lines[0], sizeof(lines[0]), "IMU  %s", t.imu_name);
      snprintf(lines[1], sizeof(lines[1]), "Акс  %+.2f %+.2f %+.2f g", t.ax, t.ay, t.az);
      snprintf(lines[2], sizeof(lines[2]), "Гир  %+.1f %+.1f %+.1f г/с", t.gx, t.gy, t.gz);
    } else {
      snprintf(lines[0], sizeof(lines[0]), "IMU  нет");
      snprintf(lines[1], sizeof(lines[1]), "Акс  —");
      snprintf(lines[2], sizeof(lines[2]), "Гир  —");
    }
    if (t.battery_percent >= 0) {
      if (t.battery_mv > 0) {
        snprintf(lines[3], sizeof(lines[3]), "Акб  %ld%%  %.2fВ%s",
                 static_cast<long>(t.battery_percent), t.battery_mv / 1000.0f,
                 t.charging ? " зар" : "");
      } else {
        snprintf(lines[3], sizeof(lines[3]), "Акб  %ld%%%s", static_cast<long>(t.battery_percent),
                 t.charging ? " зар" : "");
      }
    } else if (t.battery_mv > 0) {
      snprintf(lines[3], sizeof(lines[3]), "Акб  %.2fВ%s", t.battery_mv / 1000.0f,
               t.charging ? " зар" : "");
    } else {
      snprintf(lines[3], sizeof(lines[3]), "Акб  —");
    }
    char heap[12], flash[12];
    FormatBytes(heap, sizeof(heap), t.free_heap);
    FormatBytes(flash, sizeof(flash), t.flash_size);
    snprintf(lines[4], sizeof(lines[4]), "ОЗУ  %s своб", heap);
    if (t.has_imu_temp) {
      snprintf(lines[5], sizeof(lines[5]), "ПЗУ %s  t %.1fC", flash, t.imu_temp);
    } else {
      snprintf(lines[5], sizeof(lines[5]), "ПЗУ  %s", flash);
    }
  }

  if (hw_selected_ < 0) hw_selected_ = 0;
  if (total > 0 && hw_selected_ >= total) hw_selected_ = total - 1;
  if (hw_selected_ < hw_scroll_) hw_scroll_ = hw_selected_;
  if (hw_selected_ >= hw_scroll_ + kVisibleRows) hw_scroll_ = hw_selected_ - kVisibleRows + 1;

  for (int row = 0; row < 6; ++row) {
    const int idx = hw_scroll_ + row;
    if (row >= kVisibleRows || idx >= total) {
      lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(menu_texts_[row], lines[idx]);
    lv_obj_set_style_text_color(menu_texts_[row],
                                lv_color_hex(idx == hw_selected_ ? 0xEAFBFF : 0x9EB0C4), 0);
  }

  const int cursor_row = total > 0 ? hw_selected_ - hw_scroll_ : 0;
  if (selection_cursor_) {
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) {
      lv_obj_set_y(selection_cursor_, cursor_row * kRowStep);
    } else {
      AnimateSelectionCursor(cursor_row);
    }
  }

  if (hw_tool_ == HwToolId::Gpio) {
    lv_label_set_text(hint_label_, "Enter перекл  Del");
  } else if (hw_tool_ == HwToolId::I2c) {
    lv_label_set_text(hint_label_, "R скан  Enter шина  Del");
  } else if (hw_tool_ == HwToolId::Sensors) {
    lv_label_set_text(hint_label_, "Enter график Акс/Гир  Del");
  } else {
    lv_label_set_text(hint_label_, ";/. обновл  Del");
  }
  if (animate_in) AnimateItemsIn(kVisibleRows < total ? kVisibleRows : total, true);
}

void UiManager::HandleHwToolInput(const UiInputEvent& event) {
  using A = drivers::InputAction;
  int total = 0;
  if (hw_tool_ == HwToolId::Gpio && gpio_) total = gpio_->Count();
  else if (hw_tool_ == HwToolId::I2c && i2c_) {
    total = 2 + (i2c_->Count() == 0 ? 1 : i2c_->Count());
  } else if (hw_tool_ == HwToolId::Sensors) total = 6;

  switch (event.action) {
    case A::Up:
      if (total == 0) break;
      hw_selected_ = (hw_selected_ - 1 + total) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshHwToolScreen(false);
      break;
    case A::Down:
      if (total == 0) break;
      hw_selected_ = (hw_selected_ + 1) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshHwToolScreen(false);
      break;
    case A::Back:
      CloseHwTool();
      break;
    case A::Rescan:
      if (hw_tool_ == HwToolId::I2c && i2c_ && !i2c_->Scanning()) {
        i2c_->StartScan(i2c_->Bus());
        last_i2c_scanning_ = true;
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
        RefreshHwToolScreen(false);
      }
      break;
    case A::Select:
      if (hw_tool_ == HwToolId::Sensors) {
        if (hw_selected_ == 1) {
          OpenSensorGraph(false);
          break;
        }
        if (hw_selected_ == 2) {
          OpenSensorGraph(true);
          break;
        }
      }
      if (hw_tool_ == HwToolId::Gpio && gpio_) {
        if (gpio_->ToggleOutput(static_cast<uint8_t>(hw_selected_))) {
          if (audio_) audio_->Play(drivers::SoundId::KeyClick);
        } else if (audio_) {
          audio_->Play(drivers::SoundId::Error);
        }
        RefreshHwToolScreen(false);
      } else if (hw_tool_ == HwToolId::I2c && i2c_) {
        if (i2c_->Scanning()) break;
        if (hw_selected_ == 1) {
          const auto next = i2c_->Bus() == modules::I2cBusId::Internal
                                ? modules::I2cBusId::Grove
                                : modules::I2cBusId::Internal;
          i2c_->StartScan(next);
          last_i2c_scanning_ = true;
          if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
          RefreshHwToolScreen(false);
        } else if (hw_selected_ == 0) {
          i2c_->StartScan(i2c_->Bus());
          last_i2c_scanning_ = true;
          if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
          RefreshHwToolScreen(false);
        }
      }
      break;
    default:
      break;
  }
}

void UiManager::EnsureSensorGraphUi() {
  if (graph_panel_ != nullptr) return;

  graph_panel_ = lv_obj_create(menu_screen_);
  lv_obj_remove_style_all(graph_panel_);
  lv_obj_set_size(graph_panel_, 224, 84);
  lv_obj_align(graph_panel_, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_set_style_bg_color(graph_panel_, lv_color_hex(0x0B1220), 0);
  lv_obj_set_style_bg_opa(graph_panel_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(graph_panel_, 6, 0);
  lv_obj_set_style_border_width(graph_panel_, 1, 0);
  lv_obj_set_style_border_color(graph_panel_, lv_color_hex(kPrimaryHex), 0);
  lv_obj_set_style_border_opa(graph_panel_, LV_OPA_40, 0);
  lv_obj_add_flag(graph_panel_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(graph_panel_, LV_OBJ_FLAG_SCROLLABLE);

  // Spirit-level pad (accel)
  graph_pad_ = lv_obj_create(graph_panel_);
  lv_obj_remove_style_all(graph_pad_);
  lv_obj_set_size(graph_pad_, 72, 72);
  lv_obj_align(graph_pad_, LV_ALIGN_LEFT_MID, 6, 0);
  lv_obj_set_style_radius(graph_pad_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(graph_pad_, lv_color_hex(0x121A2A), 0);
  lv_obj_set_style_bg_opa(graph_pad_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(graph_pad_, 2, 0);
  lv_obj_set_style_border_color(graph_pad_, lv_color_hex(kPrimaryHex), 0);
  lv_obj_set_style_border_opa(graph_pad_, LV_OPA_60, 0);
  lv_obj_clear_flag(graph_pad_, LV_OBJ_FLAG_SCROLLABLE);

  graph_ring_ = lv_obj_create(graph_pad_);
  lv_obj_remove_style_all(graph_ring_);
  lv_obj_set_size(graph_ring_, 36, 36);
  lv_obj_center(graph_ring_);
  lv_obj_set_style_radius(graph_ring_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(graph_ring_, 1, 0);
  lv_obj_set_style_border_color(graph_ring_, lv_color_hex(0x3A4A62), 0);
  lv_obj_set_style_border_opa(graph_ring_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_opa(graph_ring_, LV_OPA_TRANSP, 0);

  graph_cross_h_ = lv_obj_create(graph_pad_);
  lv_obj_remove_style_all(graph_cross_h_);
  lv_obj_set_size(graph_cross_h_, 60, 1);
  lv_obj_center(graph_cross_h_);
  lv_obj_set_style_bg_color(graph_cross_h_, lv_color_hex(0x2A3A50), 0);
  lv_obj_set_style_bg_opa(graph_cross_h_, LV_OPA_COVER, 0);

  graph_cross_v_ = lv_obj_create(graph_pad_);
  lv_obj_remove_style_all(graph_cross_v_);
  lv_obj_set_size(graph_cross_v_, 1, 60);
  lv_obj_center(graph_cross_v_);
  lv_obj_set_style_bg_color(graph_cross_v_, lv_color_hex(0x2A3A50), 0);
  lv_obj_set_style_bg_opa(graph_cross_v_, LV_OPA_COVER, 0);

  graph_bubble_ = lv_obj_create(graph_pad_);
  lv_obj_remove_style_all(graph_bubble_);
  lv_obj_set_size(graph_bubble_, 12, 12);
  lv_obj_set_style_radius(graph_bubble_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(graph_bubble_, lv_color_hex(kPrimaryHex), 0);
  lv_obj_set_style_bg_opa(graph_bubble_, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(graph_bubble_, 10, 0);
  lv_obj_set_style_shadow_color(graph_bubble_, lv_color_hex(kPrimaryHex), 0);
  lv_obj_set_style_shadow_opa(graph_bubble_, LV_OPA_50, 0);
  lv_obj_align(graph_bubble_, LV_ALIGN_CENTER, 0, 0);

  static const char* kAxis[3] = {"X", "Y", "Z"};
  for (int i = 0; i < 3; ++i) {
    graph_bar_labels_[i] = lv_label_create(graph_panel_);
    lv_label_set_text(graph_bar_labels_[i], kAxis[i]);
    lv_obj_set_style_text_font(graph_bar_labels_[i], &font_ru_14, 0);
    lv_obj_set_style_text_color(graph_bar_labels_[i], lv_color_hex(0x8FA3B8), 0);

    graph_bars_[i] = lv_bar_create(graph_panel_);
    lv_obj_set_size(graph_bars_[i], 118, 12);
    lv_bar_set_range(graph_bars_[i], -100, 100);
    lv_bar_set_mode(graph_bars_[i], LV_BAR_MODE_SYMMETRICAL);
    lv_bar_set_value(graph_bars_[i], 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(graph_bars_[i], lv_color_hex(0x1A2436), 0);
    lv_obj_set_style_bg_opa(graph_bars_[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(graph_bars_[i], 4, 0);
    lv_obj_set_style_bg_color(graph_bars_[i], lv_color_hex(kPrimaryHex), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(graph_bars_[i], LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(graph_bars_[i], 4, LV_PART_INDICATOR);
  }

  graph_value_label_ = lv_label_create(graph_panel_);
  lv_obj_set_style_text_font(graph_value_label_, &font_ru_14, 0);
  lv_obj_set_style_text_color(graph_value_label_, lv_color_hex(0xEAFBFF), 0);
  lv_label_set_text(graph_value_label_, "");
  lv_obj_align(graph_value_label_, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
}

int32_t UiManager::MapSensorBar(float v, float limit) {
  if (limit < 0.001f) return 0;
  float n = v / limit;
  if (n > 1.0f) n = 1.0f;
  if (n < -1.0f) n = -1.0f;
  return static_cast<int32_t>(n * 100.0f);
}

void UiManager::OpenSensorGraph(bool gyro) {
  if (sensors_ == nullptr || !sensors_->GetTelemetry().imu_ok) {
    if (audio_) audio_->Play(drivers::SoundId::Error);
    return;
  }
  EnsureSensorGraphUi();
  in_sensor_graph_ = true;
  sensor_graph_gyro_ = gyro;
  sensors_->SetLiveMode(true);
  sensors_->Tick();
  const auto t0 = sensors_->GetTelemetry();
  if (gyro) {
    graph_sx_ = t0.gx;
    graph_sy_ = t0.gy;
    graph_sz_ = t0.gz;
  } else {
    graph_sx_ = t0.ax;
    graph_sy_ = t0.ay;
    graph_sz_ = t0.az;
    graph_bx_ = 0;
    graph_by_ = 0;
  }

  for (int i = 0; i < 6; ++i) {
    if (menu_rows_[i]) lv_obj_add_flag(menu_rows_[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (selection_cursor_) lv_obj_add_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
  if (menu_container_) lv_obj_add_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_clear_flag(graph_panel_, LV_OBJ_FLAG_HIDDEN);

  const uint32_t accent = gyro ? kSecondaryHex : kPrimaryHex;
  lv_obj_set_style_border_color(graph_panel_, lv_color_hex(accent), 0);
  lv_obj_set_style_border_color(graph_pad_, lv_color_hex(accent), 0);
  lv_obj_set_style_bg_color(graph_bubble_, lv_color_hex(accent), 0);
  lv_obj_set_style_shadow_color(graph_bubble_, lv_color_hex(accent), 0);

  if (gyro) {
    lv_obj_add_flag(graph_pad_, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 3; ++i) {
      lv_obj_set_size(graph_bars_[i], 190, 14);
      lv_obj_align(graph_bar_labels_[i], LV_ALIGN_TOP_LEFT, 8, 8 + i * 24);
      lv_obj_align(graph_bars_[i], LV_ALIGN_TOP_LEFT, 26, 8 + i * 24);
      lv_obj_set_style_bg_color(graph_bars_[i], lv_color_hex(accent), LV_PART_INDICATOR);
    }
  } else {
    lv_obj_clear_flag(graph_pad_, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 3; ++i) {
      lv_obj_set_size(graph_bars_[i], 118, 12);
      lv_obj_align(graph_bar_labels_[i], LV_ALIGN_TOP_LEFT, 88, 10 + i * 22);
      lv_obj_align(graph_bars_[i], LV_ALIGN_TOP_LEFT, 102, 10 + i * 22);
      lv_obj_set_style_bg_color(graph_bars_[i], lv_color_hex(accent), LV_PART_INDICATOR);
    }
  }

  RefreshSensorGraph();
  RefreshStatusBar();
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseSensorGraph() {
  in_sensor_graph_ = false;
  if (sensors_ != nullptr) sensors_->SetLiveMode(false);
  if (graph_panel_) lv_obj_add_flag(graph_panel_, LV_OBJ_FLAG_HIDDEN);
  if (menu_container_) lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_HIDDEN);
  RefreshHwToolScreen(true);
  RefreshStatusBar();
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::RefreshSensorGraph() {
  if (!in_sensor_graph_ || sensors_ == nullptr || graph_panel_ == nullptr) return;
  const auto t = sensors_->GetTelemetry();
  if (!t.imu_ok) {
    lv_label_set_text(hint_label_, "IMU недоступен");
    return;
  }

  float tx = 0, ty = 0, tz = 0;
  float limit = 1.0f;
  if (sensor_graph_gyro_) {
    tx = t.gx;
    ty = t.gy;
    tz = t.gz;
    limit = 250.0f;
  } else {
    tx = t.ax;
    ty = t.ay;
    tz = t.az;
    limit = 1.5f;
  }

  // Snappy follow — high alpha, no sluggish lag
  constexpr float kFollow = 0.55f;
  graph_sx_ += (tx - graph_sx_) * kFollow;
  graph_sy_ += (ty - graph_sy_) * kFollow;
  graph_sz_ += (tz - graph_sz_) * kFollow;

  const float vx = graph_sx_;
  const float vy = graph_sy_;
  const float vz = graph_sz_;

  if (!sensor_graph_gyro_) {
    constexpr float kMax = 26.0f;
    float nx = vx;
    float ny = -vy;
    if (nx > 1.0f) nx = 1.0f;
    if (nx < -1.0f) nx = -1.0f;
    if (ny > 1.0f) ny = 1.0f;
    if (ny < -1.0f) ny = -1.0f;
    const float target_bx = nx * kMax;
    const float target_by = ny * kMax;
    graph_bx_ += (target_bx - graph_bx_) * kFollow;
    graph_by_ += (target_by - graph_by_) * kFollow;
    lv_obj_set_pos(graph_bubble_,
                   static_cast<int32_t>((72 - 12) / 2 + graph_bx_),
                   static_cast<int32_t>((72 - 12) / 2 + graph_by_));

    const bool centered = (fabsf(nx) < 0.08f && fabsf(ny) < 0.08f);
    lv_obj_set_style_bg_color(graph_bubble_,
                              lv_color_hex(centered ? 0x5CFF9A : kPrimaryHex), 0);
  }

  lv_bar_set_value(graph_bars_[0], MapSensorBar(vx, limit), LV_ANIM_OFF);
  lv_bar_set_value(graph_bars_[1], MapSensorBar(vy, limit), LV_ANIM_OFF);
  lv_bar_set_value(graph_bars_[2], MapSensorBar(vz, limit), LV_ANIM_OFF);

  if (sensor_graph_gyro_) {
    lv_label_set_text_fmt(graph_value_label_, "%+.0f %+.0f %+.0f", vx, vy, vz);
    lv_label_set_text(hint_label_, "г/с   Del назад");
  } else {
    lv_label_set_text_fmt(graph_value_label_, "%+.2f %+.2f %+.2f g", vx, vy, vz);
    lv_label_set_text(hint_label_, "уровень   Del назад");
  }
}

void UiManager::OpenFsBrowser() {
  in_fs_browser_ = true;
  fs_selected_ = 0;
  fs_scroll_ = 0;
  LayoutListRows(kRowStep);
  if (storage_ != nullptr) {
    storage_->SetVolume(services::FsVolume::Internal);
    storage_->List(services::FsVolume::Internal, "/");
  }
  RefreshFsBrowser(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseFsBrowser() {
  in_fs_browser_ = false;
  LayoutListRows(kRowStep);
  RefreshMenuList(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::RefreshFsBrowser(bool animate_in) {
  LayoutListRows(kRowStep);
  char lines[28][40];
  int total = 0;

  if (storage_ == nullptr) {
    snprintf(lines[0], sizeof(lines[0]), "Нет хранилища");
    total = 1;
  } else {
    const auto vol = storage_->CurrentVolume();
    const auto st = storage_->GetStats(vol);
    char tot[12], used[12], freeb[12], chip[12];
    FormatBytes(tot, sizeof(tot), st.total_bytes);
    FormatBytes(used, sizeof(used), st.used_bytes);
    FormatBytes(freeb, sizeof(freeb), st.free_bytes);
    FormatBytes(chip, sizeof(chip), storage_->ChipFlashBytes());

    snprintf(lines[0], sizeof(lines[0]), "[%s] %s", st.label, st.mounted ? "ок" : "—");
    snprintf(lines[1], sizeof(lines[1]), "Всего %s  занято %s", tot, used);
    snprintf(lines[2], sizeof(lines[2]), "Своб %s  чип %s", freeb, chip);
    snprintf(lines[3], sizeof(lines[3]), "Том  %s",
             vol == services::FsVolume::Internal ? "Флеш" : "SD");
    snprintf(lines[4], sizeof(lines[4]), "Путь %s", storage_->CurrentPath());
    total = 5;

    const uint8_t n = storage_->EntryCount();
    if (n == 0) {
      snprintf(lines[total++], sizeof(lines[0]), "(пусто)");
    } else {
      for (uint8_t i = 0; i < n && total < 28; ++i) {
        const auto& e = storage_->EntryAt(i);
        if (e.is_dir) {
          snprintf(lines[total], sizeof(lines[total]), "/%s", e.name);
        } else {
          char sz[10];
          FormatBytes(sz, sizeof(sz), e.size);
          char name[22];
          TruncateText(name, sizeof(name), e.name, 16);
          snprintf(lines[total], sizeof(lines[total]), "%s %s", name, sz);
        }
        ++total;
      }
    }
  }

  if (fs_selected_ < 0) fs_selected_ = 0;
  if (fs_selected_ >= total) fs_selected_ = total - 1;
  if (fs_selected_ < fs_scroll_) fs_scroll_ = fs_selected_;
  if (fs_selected_ >= fs_scroll_ + kVisibleRows) fs_scroll_ = fs_selected_ - kVisibleRows + 1;

  for (int row = 0; row < 6; ++row) {
    const int idx = fs_scroll_ + row;
    if (row >= kVisibleRows || idx >= total) {
      lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(menu_texts_[row], lines[idx]);
    lv_obj_set_style_text_color(menu_texts_[row],
                                lv_color_hex(idx == fs_selected_ ? 0xEAFBFF : 0x9EB0C4), 0);
  }

  const int cursor_row = fs_selected_ - fs_scroll_;
  if (selection_cursor_) {
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) lv_obj_set_y(selection_cursor_, cursor_row * kRowStep);
    else AnimateSelectionCursor(cursor_row);
  }

  lv_label_set_text(hint_label_, "Enter открыть  R SD/флеш  Del");
  if (animate_in) AnimateItemsIn(kVisibleRows, true);
}

void UiManager::HandleFsBrowserInput(const UiInputEvent& event) {
  using A = drivers::InputAction;
  if (storage_ == nullptr) {
    if (event.action == A::Back) CloseFsBrowser();
    return;
  }

  // header rows 0..4, files start at 5
  const int header = 5;
  const int file_count = storage_->EntryCount() == 0 ? 1 : storage_->EntryCount();
  const int total = header + file_count;

  switch (event.action) {
    case A::Up:
      fs_selected_ = (fs_selected_ - 1 + total) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshFsBrowser(false);
      break;
    case A::Down:
      fs_selected_ = (fs_selected_ + 1) % total;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshFsBrowser(false);
      break;
    case A::Back:
      if (strcmp(storage_->CurrentPath(), "/") != 0) {
        storage_->GoUp();
        fs_selected_ = 0;
        fs_scroll_ = 0;
        RefreshFsBrowser(false);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      } else {
        CloseFsBrowser();
      }
      break;
    case A::Rescan: {
      const auto next = storage_->CurrentVolume() == services::FsVolume::Internal
                            ? services::FsVolume::Sd
                            : services::FsVolume::Internal;
      if (next == services::FsVolume::Sd) {
        if (!storage_->MountSd()) {
          if (audio_) audio_->Play(drivers::SoundId::Error);
          lv_label_set_text(hint_label_, "SD нет / CS=nRF");
          break;
        }
      }
      storage_->SetVolume(next);
      storage_->List(next, "/");
      fs_selected_ = 0;
      fs_scroll_ = 0;
      if (audio_) audio_->Play(drivers::SoundId::Success);
      RefreshFsBrowser(false);
      break;
    }
    case A::Select:
      if (fs_selected_ == 3) {
        // toggle volume same as R
        PostAction(A::Rescan);
        break;
      }
      if (fs_selected_ >= header && storage_->EntryCount() > 0) {
        const int idx = fs_selected_ - header;
        const auto& e = storage_->EntryAt(static_cast<uint8_t>(idx));
        if (e.is_dir) {
          if (storage_->EnterDir(e.name)) {
            fs_selected_ = 0;
            fs_scroll_ = 0;
            if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
            RefreshFsBrowser(false);
          }
        } else if (audio_) {
          audio_->Play(drivers::SoundId::KeyClick);
        }
      }
      break;
    default:
      break;
  }
}

void UiManager::OpenAbout() {
  in_about_screen_ = true;
  about_selected_ = 0;
  LayoutListRows(kRowStep);
  RefreshAboutScreen(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::CloseAbout() {
  in_about_screen_ = false;
  LayoutListRows(kRowStep);
  RefreshMenuList(true);
  if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
}

void UiManager::RefreshAboutScreen(bool animate_in) {
  LayoutListRows(kRowStep);
  char lines[6][40];
  char flash[12], heap[12];
  FormatBytes(flash, sizeof(flash), ESP.getFlashChipSize());
  FormatBytes(heap, sizeof(heap), ESP.getFreeHeap());
  snprintf(lines[0], sizeof(lines[0]), "AxiomOS  v%s", axiom::kProjectVersion);
  snprintf(lines[1], sizeof(lines[1]), "чип ESP32-S3");
  snprintf(lines[2], sizeof(lines[2]), "ПЗУ  %s", flash);
  snprintf(lines[3], sizeof(lines[3]), "ОЗУ  %s своб", heap);
  if (storage_ != nullptr && storage_->InternalMounted()) {
    const auto st = storage_->GetStats(services::FsVolume::Internal);
    char tot[10], used[10];
    FormatBytes(tot, sizeof(tot), st.total_bytes);
    FormatBytes(used, sizeof(used), st.used_bytes);
    snprintf(lines[4], sizeof(lines[4]), "ФС   %s/%s", used, tot);
  } else {
    snprintf(lines[4], sizeof(lines[4]), "ФС   —");
  }
  snprintf(lines[5], sizeof(lines[5]), "Назад");

  constexpr int total = 6;
  if (about_selected_ < 0) about_selected_ = 0;
  if (about_selected_ >= total) about_selected_ = total - 1;
  static int about_scroll = 0;
  if (about_selected_ < about_scroll) about_scroll = about_selected_;
  if (about_selected_ >= about_scroll + kVisibleRows) about_scroll = about_selected_ - kVisibleRows + 1;

  for (int row = 0; row < 6; ++row) {
    const int idx = about_scroll + row;
    if (row >= kVisibleRows || idx >= total) {
      lv_obj_add_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(menu_rows_[row], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(menu_texts_[row], lines[idx]);
    lv_obj_set_style_text_color(
        menu_texts_[row], lv_color_hex(idx == about_selected_ ? 0xEAFBFF : 0x9EB0C4), 0);
  }
  const int cursor_row = about_selected_ - about_scroll;
  if (selection_cursor_) {
    lv_obj_clear_flag(selection_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (animate_in) lv_obj_set_y(selection_cursor_, cursor_row * kRowStep);
    else AnimateSelectionCursor(cursor_row);
  }
  lv_label_set_text(hint_label_, "Del назад");
  if (animate_in) AnimateItemsIn(kVisibleRows, true);
}

void UiManager::HandleAboutInput(drivers::InputAction action) {
  switch (action) {
    case drivers::InputAction::Up:
      about_selected_ = (about_selected_ - 1 + 6) % 6;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshAboutScreen(false);
      break;
    case drivers::InputAction::Down:
      about_selected_ = (about_selected_ + 1) % 6;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      RefreshAboutScreen(false);
      break;
    case drivers::InputAction::Back:
    case drivers::InputAction::Select:
      if (action == drivers::InputAction::Select && about_selected_ != 5) break;
      CloseAbout();
      break;
    default:
      break;
  }
}

void UiManager::HandleSettingsInput(drivers::InputAction action) {
  if (settings_edit_mode_) {
    if (action == drivers::InputAction::Select || action == drivers::InputAction::Back) {
      settings_edit_mode_ = false;
      if (audio_) audio_->Play(drivers::SoundId::Success);
      return;
    }
    if (action != drivers::InputAction::Up && action != drivers::InputAction::Down) return;
    const int dir = action == drivers::InputAction::Up ? 1 : -1;
    switch (settings_index_) {
      case 0:
        settings_.brightness =
            static_cast<uint8_t>(constrain(settings_.brightness + dir * 5, 10, 255));
        break;
      case 1:
        settings_.volume = static_cast<uint8_t>(constrain(settings_.volume + dir * 4, 0, 255));
        break;
      case 2:
        settings_.theme = settings_.theme == services::ThemeMode::Cyberpunk
                              ? services::ThemeMode::Dark
                              : services::ThemeMode::Cyberpunk;
        break;
      case 3:
        settings_.rf_channel =
            static_cast<uint8_t>(constrain(settings_.rf_channel + dir, 0, 125));
        break;
      case 4:
        settings_.rf_power = static_cast<uint8_t>(constrain(settings_.rf_power + dir, 0, 3));
        break;
      default:
        break;
    }
    settings_dirty_ = true;
    if (audio_) audio_->Play(drivers::SoundId::KeyClick);
    return;
  }

  switch (action) {
    case drivers::InputAction::Up:
      settings_index_ = (settings_index_ - 1 + 6) % 6;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      break;
    case drivers::InputAction::Down:
      settings_index_ = (settings_index_ + 1) % 6;
      if (audio_) audio_->Play(drivers::SoundId::KeyClick);
      break;
    case drivers::InputAction::Back:
      in_settings_screen_ = false;
      settings_edit_mode_ = false;
      RefreshMenuList(true);
      if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      break;
    case drivers::InputAction::Select:
      if (settings_index_ == 5) {
        in_settings_screen_ = false;
        RefreshMenuList(true);
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      } else {
        settings_edit_mode_ = true;
        if (audio_) audio_->Play(drivers::SoundId::MenuOpen);
      }
      break;
    default:
      break;
  }
}

}  // namespace axiom::ui
