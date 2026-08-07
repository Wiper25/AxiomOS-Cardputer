#include "drivers/display/display_driver.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <esp_heap_caps.h>

namespace axiom::drivers {

static uint32_t LvglTickMs() { return millis(); }

bool DisplayDriver::Begin() {
  lv_init();
  lv_tick_set_cb(LvglTickMs);

  const uint16_t width = M5.Display.width();
  const uint16_t height = M5.Display.height();
  draw_buffer_pixels_ = static_cast<size_t>(width) * 24;  // 24 lines buffer
  draw_buffer_ =
      static_cast<lv_color_t*>(heap_caps_malloc(draw_buffer_pixels_ * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

  if (draw_buffer_ == nullptr) {
    return false;
  }

  lv_display_ = lv_display_create(width, height);
  if (lv_display_ == nullptr) {
    return false;
  }

  lv_display_set_flush_cb(lv_display_, FlushCallback);
  lv_display_set_buffers(lv_display_, draw_buffer_, nullptr,
                         draw_buffer_pixels_ * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  return true;
}

uint16_t DisplayDriver::Width() const { return M5.Display.width(); }

uint16_t DisplayDriver::Height() const { return M5.Display.height(); }

void DisplayDriver::FlushCallback(lv_display_t* disp, const lv_area_t* area,
                                  uint8_t* color_map) {
  (void)disp;
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;

  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, width, height);
  M5.Display.pushPixels(reinterpret_cast<uint16_t*>(color_map),
                        static_cast<uint32_t>(width) * height, true);
  M5.Display.endWrite();

  lv_display_flush_ready(disp);
}

}  // namespace axiom::drivers
