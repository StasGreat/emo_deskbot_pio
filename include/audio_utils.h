#pragma once

#include <Arduino.h>
#include <math.h>
#include "config.h"

static inline int16_t clamp16(int32_t x) {
  if (x > 32767) return 32767;
  if (x < -32768) return -32768;
  return (int16_t)x;
}

static inline int16_t mic32ToS16(int32_t v) {
  int32_t s = (int32_t)(v >> MIC_SHIFT);
  if (MIC_GAIN != 1.0f) {
    s = (int32_t)lroundf((float)s * MIC_GAIN);
  }
  return clamp16(s);
}
