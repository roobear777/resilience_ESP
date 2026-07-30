#include "led_animations.h"

#include <math.h>

float ledTriangleWave(float progress) {
  float wrappedProgress = progress - floorf(progress);

  if (wrappedProgress < 0.0f) {
    wrappedProgress += 1.0f;
  }

  if (wrappedProgress < 0.5f) {
    return wrappedProgress * 2.0f;
  }

  return (1.0f - wrappedProgress) * 2.0f;
}

float ledPeakRampBrightness(
  float elapsedMs,
  float startMs,
  float peakMs,
  float holdMs,
  float rampDownMs,
  float baseBrightness,
  float peakBrightness
) {
  float localElapsedMs = elapsedMs - startMs;

  if (localElapsedMs < 0.0f) {
    return -1.0f;
  }

  float peakDurationMs = peakMs + holdMs;
  float activeDurationMs = peakDurationMs + rampDownMs;

  if (localElapsedMs >= activeDurationMs) {
    return -1.0f;
  }

  if (localElapsedMs < peakDurationMs || rampDownMs <= 0.0f) {
    return peakBrightness;
  }

  float rampElapsedMs = localElapsedMs - peakDurationMs;
  float rampProgress = rampElapsedMs / rampDownMs;
  float brightness = peakBrightness - ((peakBrightness - baseBrightness) * rampProgress);

  if (brightness < baseBrightness) {
    return baseBrightness;
  }

  return brightness;
}
