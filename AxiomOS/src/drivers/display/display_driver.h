#pragma once

#include <lvgl.h>

namespace axiom::drivers {

class DisplayDriver {
 public:
  bool Begin();
  uint16_t Width() const;
  uint16_t Height() const;
  lv_display_t* LvDisplay() const { return lv_display_; }

 private:
  static void FlushCallback(lv_display_t* disp, const lv_area_t* area,
                            uint8_t* color_map);

  lv_display_t* lv_display_ = nullptr;
  lv_color_t* draw_buffer_ = nullptr;
  size_t draw_buffer_pixels_ = 0;
};

}  // namespace axiom::drivers
