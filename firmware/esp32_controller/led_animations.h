#ifndef TARDI_LED_ANIMATIONS_H
#define TARDI_LED_ANIMATIONS_H

float ledTriangleWave(float progress);

float ledPeakRampBrightness(
  float elapsedMs,
  float startMs,
  float peakMs,
  float holdMs,
  float rampDownMs,
  float baseBrightness,
  float peakBrightness
);

#endif
