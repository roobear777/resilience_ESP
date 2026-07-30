#ifndef TARDI_LED_COLOR_CONVERT_H
#define TARDI_LED_COLOR_CONVERT_H

#include <Arduino.h>

#include "led_color.h"

struct LedRgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

LedRgbColor ledColorToRgb(const LedColor &color);

#endif
