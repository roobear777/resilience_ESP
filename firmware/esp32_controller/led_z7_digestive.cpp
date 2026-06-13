#include "led_z7_digestive.h"

#include "led_layout.h"

const float Z7_GLOBAL_BRIGHTNESS = 1.0f;

const float Z7_HUE = 0.0f;
const float Z7_SATURATION = 1.0f;

const float Z7_AMBIENT_BASELINE_BRIGHTNESS = 0.25f;
const float Z7_AMBIENT_PEAK_BRIGHTNESS = 1.0f;
const float Z7_AMBIENT_GRADIENT_LENGTH = 20.0f;
const float Z7_AMBIENT_FALL_DURATION_MS = 2000.0f;
const float Z7_AMBIENT_PAUSE_MS = 500.0f;

const float Z7_ACTIVE_BASELINE_BRIGHTNESS = 0.35f;
const float Z7_ACTIVE_PEAK_BRIGHTNESS = 1.0f;
const float Z7_ACTIVE_GRADIENT_LENGTH = 20.0f;
const float Z7_ACTIVE_FALL_DURATION_MS = 700.0f;
const float Z7_ACTIVE_PAUSE_MS = 100.0f;

static float z7AmbientElapsedMs = 0.0f;
static float z7ActiveElapsedMs = 0.0f;

static float ledZ7DigestivePulseBrightness(
  uint16_t localIndex,
  float elapsedMs,
  float fallDurationMs,
  float gradientLength,
  float baselineBrightness,
  float peakBrightness
) {
  uint16_t physicalPosition = localIndex % LED_Z7_PIXEL_COUNT;
  float visualPosition = (LED_Z7_PIXEL_COUNT - 1) - physicalPosition;
  float brightness = baselineBrightness;

  if (elapsedMs < fallDurationMs) {
    float peakPosition = (elapsedMs / fallDurationMs) * (LED_Z7_PIXEL_COUNT + gradientLength) - gradientLength;
    float distanceFromPeak = visualPosition - peakPosition;

    if (distanceFromPeak >= 0.0f && distanceFromPeak <= gradientLength) {
      float halfWidth = gradientLength / 2.0f;
      float rampAmount = 0.0f;

      if (distanceFromPeak < halfWidth) {
        rampAmount = distanceFromPeak / halfWidth;
      } else {
        rampAmount = (gradientLength - distanceFromPeak) / halfWidth;
      }

      brightness = baselineBrightness + ((peakBrightness - baselineBrightness) * rampAmount);
    }
  }

  return brightness;
}

static LedColor ledZ7DigestiveColor(float brightness) {
  LedColor color = {
    Z7_HUE,
    Z7_SATURATION,
    brightness * Z7_GLOBAL_BRIGHTNESS
  };

  return color;
}

void ledZ7DigestiveBegin() {
  z7AmbientElapsedMs = 0.0f;
  z7ActiveElapsedMs = 0.0f;
}

void ledZ7DigestiveUpdate(uint32_t deltaMs) {
  z7AmbientElapsedMs += deltaMs;
  float ambientCycleTotalMs = Z7_AMBIENT_FALL_DURATION_MS + Z7_AMBIENT_PAUSE_MS;

  while (z7AmbientElapsedMs >= ambientCycleTotalMs) {
    z7AmbientElapsedMs -= ambientCycleTotalMs;
  }

  z7ActiveElapsedMs += deltaMs;
  float activeCycleTotalMs = Z7_ACTIVE_FALL_DURATION_MS + Z7_ACTIVE_PAUSE_MS;

  while (z7ActiveElapsedMs >= activeCycleTotalMs) {
    z7ActiveElapsedMs -= activeCycleTotalMs;
  }
}

LedColor ledZ7DigestiveRender(uint16_t localIndex, bool active, uint32_t nowMs) {
  (void)nowMs;

  if (localIndex >= LED_ZONE_PIXEL_COUNT[LED_ZONE_Z7_DIGESTIVE]) {
    return LED_COLOR_BLACK;
  }

  float brightness = active ?
    ledZ7DigestivePulseBrightness(
      localIndex,
      z7ActiveElapsedMs,
      Z7_ACTIVE_FALL_DURATION_MS,
      Z7_ACTIVE_GRADIENT_LENGTH,
      Z7_ACTIVE_BASELINE_BRIGHTNESS,
      Z7_ACTIVE_PEAK_BRIGHTNESS
    ) :
    ledZ7DigestivePulseBrightness(
      localIndex,
      z7AmbientElapsedMs,
      Z7_AMBIENT_FALL_DURATION_MS,
      Z7_AMBIENT_GRADIENT_LENGTH,
      Z7_AMBIENT_BASELINE_BRIGHTNESS,
      Z7_AMBIENT_PEAK_BRIGHTNESS
    );

  return ledZ7DigestiveColor(brightness);
}
