#include "led_color_convert.h"

#include <math.h>

static float ledClamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }

  if (value > 1.0f) {
    return 1.0f;
  }

  return value;
}

static float ledWrap01(float value) {
  float wrapped = value - floorf(value);

  if (wrapped < 0.0f) {
    wrapped += 1.0f;
  }

  return wrapped;
}

static uint8_t ledFloatToByte(float value) {
  return uint8_t((ledClamp01(value) * 255.0f) + 0.5f);
}

LedRgbColor ledColorToRgb(const LedColor &color) {
  float h = ledWrap01(color.h);
  float s = ledClamp01(color.s);
  float v = ledClamp01(color.v);

  if (v <= 0.0f) {
    return { 0, 0, 0 };
  }

  if (s <= 0.0f) {
    uint8_t gray = ledFloatToByte(v);
    return { gray, gray, gray };
  }

  float scaledHue = h * 6.0f;
  int sector = int(floorf(scaledHue));
  float fraction = scaledHue - sector;
  float p = v * (1.0f - s);
  float q = v * (1.0f - (s * fraction));
  float t = v * (1.0f - (s * (1.0f - fraction)));

  switch (sector % 6) {
    case 0:
      return { ledFloatToByte(v), ledFloatToByte(t), ledFloatToByte(p) };
    case 1:
      return { ledFloatToByte(q), ledFloatToByte(v), ledFloatToByte(p) };
    case 2:
      return { ledFloatToByte(p), ledFloatToByte(v), ledFloatToByte(t) };
    case 3:
      return { ledFloatToByte(p), ledFloatToByte(q), ledFloatToByte(v) };
    case 4:
      return { ledFloatToByte(t), ledFloatToByte(p), ledFloatToByte(v) };
    default:
      return { ledFloatToByte(v), ledFloatToByte(p), ledFloatToByte(q) };
  }
}
