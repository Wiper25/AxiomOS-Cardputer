#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_USE_LOG 0

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (96U * 1024U)

#define LV_USE_ANIMATION 1
#define LV_USE_PERF_MONITOR 0

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#endif
