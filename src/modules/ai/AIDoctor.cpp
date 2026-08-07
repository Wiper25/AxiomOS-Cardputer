#include "modules/ai/AIDoctor.h"

#include <stdio.h>
#include <string.h>

#include "modules/mqtt/mqtt_module.h"
#include "modules/nrf24/nrf24_module.h"
#include "modules/sensors/sensors_module.h"
#include "modules/wifi/wifi_module.h"

namespace axiom::ai {

bool AIDoctor::Diagnose(DoctorFinding* out, uint8_t max_out, uint8_t& count) {
  count = 0;
  if (!out || max_out == 0) return false;

  auto add = [&](uint8_t sev, const char* p, const char* c, const char* f) {
    if (count >= max_out) return;
    DoctorFinding& d = out[count++];
    d.severity = sev;
    strncpy(d.problem, p, sizeof(d.problem) - 1);
    strncpy(d.cause, c, sizeof(d.cause) - 1);
    strncpy(d.fix, f, sizeof(d.fix) - 1);
  };

  if (sensors_) {
    const auto t = sensors_->GetTelemetry();
    if (t.free_heap < 35000U) {
      add(2, "Мало свободной RAM", "Утечка / тяжёлые буферы LVGL/сети",
          "Перезагрузка; закрой спектр/граф");
    }
    if (t.battery_mv > 0 && t.battery_mv < 3400 && !t.charging) {
      add(2, "Низкое напряжение батареи", "Нестабильное питание / разряд",
          "Проверь питание 3.3V / зарядку");
    }
  }

  if (wifi_) {
    const auto w = wifi_->GetTelemetry();
    if (!w.connected) {
      add(1, "WiFi не подключён", "Нет STA / неверный пароль / далеко AP",
          "Сканер WiFi → Connect");
    } else if (w.link_rssi < -80) {
      add(1, "Слабый WiFi сигнал", "Расстояние / помехи 2.4GHz", "Сменить место или канал AP");
    }
  }

  if (nrf_) {
    const auto r = nrf_->GetTelemetry();
    if (!r.present) {
      add(2, "nRF24 отсутствует", "SPI/CS/питание модуля", "Проверь PA+LNA и проводку");
    } else if (r.activity_percent > 80) {
      add(1, "Сильные помехи RF", "Пересечение с WiFi каналами",
          "Смени RF канал (RfAgent рекомендует)");
    }
  }

  if (count == 0) {
    add(0, "Проблем не найдено", "Телеметрия в норме", "Можно работать с AI/радио");
  }
  return true;
}

bool AIDoctor::DiagnoseText(char* dst, size_t n) {
  if (!dst || n == 0) return false;
  DoctorFinding findings[6];
  uint8_t c = 0;
  Diagnose(findings, 6, c);
  size_t off = 0;
  for (uint8_t i = 0; i < c; ++i) {
    const int w =
        snprintf(dst + off, n - off, "%u) %s\nПричина: %s\nФикс: %s\n", i + 1,
                 findings[i].problem, findings[i].cause, findings[i].fix);
    if (w < 0 || static_cast<size_t>(w) >= n - off) break;
    off += static_cast<size_t>(w);
  }
  return off > 0;
}

}  // namespace axiom::ai
