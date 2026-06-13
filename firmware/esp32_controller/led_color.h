#ifndef TARDI_LED_COLOR_H
#define TARDI_LED_COLOR_H

struct LedColor {
  float h;
  float s;
  float v;
};

const LedColor LED_COLOR_BLACK = { 0.0f, 0.0f, 0.0f };

#endif
