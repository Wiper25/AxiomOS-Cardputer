#include "modules/ai/knowledge/kb_builtin.h"

#include "modules/ai/AIKnowledge.h"

namespace axiom::ai {
namespace {

const KnowledgeArticle kArticles[] = {
    {"esp32", "ESP32-S3", "esp32 s3 dual core wifi bluetooth freertos heap",
     "ESP32-S3: dual-core Xtensa, WiFi/BLE, FreeRTOS. Следи за heap "
     "(ESP.getFreeHeap). Не блокируй UI task долгими delay. Тяжёлую сеть "
     "выноси в отдельный task. PSRAM опционален."},

    {"cardputer", "Cardputer ADV", "cardputer m5stack keyboard display stamp",
     "M5Stack Cardputer ADV: ESP32-S3, встроенная клавиатура, дисплей ST7789, "
     "динамик, слот microSD, Grove I2C. AxiomOS — кастомная LVGL-прошивка."},

    {"nrf24", "nRF24L01+", "nrf24 rf24 radio 2.4ghz channel pa lna spi",
     "nRF24L01+ PA+LNA: 2.4 GHz, каналы 0-125. Высокая активность канала = "
     "помехи (WiFi 1/6/11). Снизь PA при ближней связи. SPI: CE/CSN по пинам "
     "AxiomOS config. Сканер спектра показывает загрузку каналов."},

    {"wifi", "WiFi", "wifi wlan ssid rssi scan connect station ap",
     "WiFi STA: сканируй сети, смотри RSSI. <-70 dBm слабо. Шифрование WPA2. "
     "При reconnect проверь пароль и канал AP. MQTT/HTTP требуют connected STA."},

    {"bluetooth", "Bluetooth", "bluetooth ble bt classic advertising",
     "На Cardputer BT используется для телеметрии статуса. Полный BLE stack "
     "тяжёлый по RAM — держи включение минимальным. Конфликты с WiFi возможны "
     "на одном радио."},

    {"mqtt", "MQTT", "mqtt broker publish subscribe iot topic",
     "MQTT: TCP к брокеру, client_id уникален. Sub topic с # для wildcard. "
     "QoS 0 по умолчанию в PubSubClient. При обрыве WiFi — reconnect в Tick."},

    {"gpio", "GPIO", "gpio pin digital input output pullup",
     "GPIO монитор читает уровни пинов. Не гоняй конфликтующие пины SPI/I2C. "
     "3.3V логика. Внешние подтяжки для плавающих входов."},

    {"spi", "SPI", "spi mosi miso sck cs bus frequency",
     "SPI на Cardputer ADV делит шину с nRF/SD. Конфликты CS критичны. "
     "Частота слишком высокая = ошибки SD/nRF. Используй отдельные CS."},

    {"i2c", "I2C", "i2c sda scl grove scan address",
     "Grove I2C: SDA=2 SCL=1. Сканер ищет ACK на адресах 0x08-0x77. "
     "Нет устройств — проверь питание модуля и подтяжки 4.7k."},

    {"lvgl", "LVGL", "lvgl ui display widget animation flush",
     "LVGL 9 на Cardputer: tick в UI task ~5ms. Не вызывай lv_* из других "
     "core без блокировки. Буфер отрисовки во внутреннем RAM. Анимации "
     "лёгкие — экран 240x135."},

    {"power", "Питание", "power battery voltage reboot brownout 3.3v",
     "Перезагрузки часто = brownout / слабое питание 3.3V / USB ток. "
     "PA+LNA nRF жрёт ток — снизь мощность. Смотри battery_mv и charging."},

    {"axiom", "AxiomOS AI", "axiom ai chat agent knowledge memory",
     "AxiomOS AI: чат через внешний Python LLM-сервер (REST/WS). Локальная "
     "KB отвечает без сети. Агенты анализируют WiFi/RF/устройство. Опасные "
     "действия требуют подтверждения."},
};

}  // namespace

const KnowledgeArticle* BuiltinKnowledge(uint16_t& count) {
  count = static_cast<uint16_t>(sizeof(kArticles) / sizeof(kArticles[0]));
  return kArticles;
}

}  // namespace axiom::ai
