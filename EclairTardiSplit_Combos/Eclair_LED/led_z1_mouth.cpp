#include "led_z1_mouth.h"

#include "led_animations.h"
#include "led_layout.h"

const float LED_GLOBAL_BRIGHTNESS = 1.0f;

const float Z1_AMBIENT_BASE_BRIGHTNESS = 0.04f;
const float Z1_AMBIENT_PEAK_BRIGHTNESS = 0.20f;
const float Z1_AMBIENT_RAMP_UP_MS = 200.0f;
const float Z1_AMBIENT_PEAK_MS = 100.0f;
const float Z1_AMBIENT_RAMP_DOWN_MS = 300.0f;
const float Z1_AMBIENT_STRIP_START_OFFSET_MS = 150.0f;
const float Z1_AMBIENT_PAUSE_MS = 600.0f;

const float Z1_ACTIVE_BASE_BRIGHTNESS = 0.01f;
const float Z1_ACTIVE_PEAK_BRIGHTNESS = 0.90f;
const float Z1_ACTIVE_PEAK_MS = 8.0f;
const float Z1_ACTIVE_RAMP_DOWN_MS = 600.0f;
const float Z1_ACTIVE_STRIP_START_OFFSET_MS = 100.0f;
const float Z1_ACTIVE_REPEAT_INTERVAL_MS = 200.0f;

static float z1AmbientElapsedMs = 0.0f;
static float z1ActiveElapsedMs = 0.0f;

static uint8_t ledZ1MouthStripIndexFor(uint16_t localIndex) {
  uint16_t cumulativePixels = 0;

  for (uint8_t stripIndex = 0; stripIndex < LED_Z1_STRIP_COUNT; stripIndex++) {
    cumulativePixels += LED_Z1_STRIP_LENGTHS[stripIndex];

    if (localIndex < cumulativePixels) {
      return stripIndex;
    }
  }

  return LED_Z1_STRIP_COUNT - 1;
}

static float ledZ1MouthRenderAmbientBrightness(uint16_t localIndex) {
  uint8_t stripIndex = ledZ1MouthStripIndexFor(localIndex);
  float stripStartMs = stripIndex * Z1_AMBIENT_STRIP_START_OFFSET_MS;
  float elapsedSinceStripStartMs = z1AmbientElapsedMs - stripStartMs;
  float brightness = Z1_AMBIENT_BASE_BRIGHTNESS;

  if (elapsedSinceStripStartMs < 0.0f) {
    return brightness;
  }

  if (elapsedSinceStripStartMs < Z1_AMBIENT_RAMP_UP_MS) {
    float progress = elapsedSinceStripStartMs / Z1_AMBIENT_RAMP_UP_MS;
    return Z1_AMBIENT_BASE_BRIGHTNESS +
      ((Z1_AMBIENT_PEAK_BRIGHTNESS - Z1_AMBIENT_BASE_BRIGHTNESS) * progress);
  }

  if (elapsedSinceStripStartMs < Z1_AMBIENT_RAMP_UP_MS + Z1_AMBIENT_PEAK_MS) {
    return Z1_AMBIENT_PEAK_BRIGHTNESS;
  }

  if (elapsedSinceStripStartMs < Z1_AMBIENT_RAMP_UP_MS + Z1_AMBIENT_PEAK_MS + Z1_AMBIENT_RAMP_DOWN_MS) {
    float rampDownElapsedMs = elapsedSinceStripStartMs - Z1_AMBIENT_RAMP_UP_MS - Z1_AMBIENT_PEAK_MS;
    float progress = rampDownElapsedMs / Z1_AMBIENT_RAMP_DOWN_MS;
    brightness = Z1_AMBIENT_PEAK_BRIGHTNESS -
      ((Z1_AMBIENT_PEAK_BRIGHTNESS - Z1_AMBIENT_BASE_BRIGHTNESS) * progress);
  }

  return brightness;
}

static float ledZ1MouthRenderActiveBrightness(uint16_t localIndex) {
  uint8_t stripIndex = ledZ1MouthStripIndexFor(localIndex);
  float stripActivationMs = stripIndex * Z1_ACTIVE_STRIP_START_OFFSET_MS;
  float brightness = ledPeakRampBrightness(
    z1ActiveElapsedMs,
    stripActivationMs,
    Z1_ACTIVE_PEAK_MS,
    0.0f,
    Z1_ACTIVE_RAMP_DOWN_MS,
    Z1_ACTIVE_BASE_BRIGHTNESS,
    Z1_ACTIVE_PEAK_BRIGHTNESS
  );

  if (brightness < 0.0f) {
    return Z1_ACTIVE_BASE_BRIGHTNESS;
  }

  return brightness;
}

void ledZ1MouthBegin() {
  z1AmbientElapsedMs = 0.0f;
  z1ActiveElapsedMs = 0.0f;
}

void ledZ1MouthUpdate(uint32_t deltaMs) {
  z1AmbientElapsedMs += deltaMs;
  z1ActiveElapsedMs += deltaMs;

  float ambientRingDurationMs = Z1_AMBIENT_RAMP_UP_MS + Z1_AMBIENT_PEAK_MS + Z1_AMBIENT_RAMP_DOWN_MS;
  float ambientFullCycleMs =
    ((LED_Z1_STRIP_COUNT - 1) * Z1_AMBIENT_STRIP_START_OFFSET_MS) +
    ambientRingDurationMs +
    Z1_AMBIENT_PAUSE_MS;

  while (z1AmbientElapsedMs >= ambientFullCycleMs) {
    z1AmbientElapsedMs -= ambientFullCycleMs;
  }

  float activeFullDurationMs =
    ((LED_Z1_STRIP_COUNT - 1) * Z1_ACTIVE_STRIP_START_OFFSET_MS) +
    Z1_ACTIVE_PEAK_MS +
    Z1_ACTIVE_RAMP_DOWN_MS;
  float activeCycleMs = activeFullDurationMs;

  if (Z1_ACTIVE_REPEAT_INTERVAL_MS > activeCycleMs) {
    activeCycleMs = Z1_ACTIVE_REPEAT_INTERVAL_MS;
  }

  while (z1ActiveElapsedMs >= activeCycleMs) {
    z1ActiveElapsedMs -= activeCycleMs;
  }
}

LedColor ledZ1MouthRender(uint16_t localIndex, bool active, uint32_t nowMs) {
  (void)nowMs;

  if (localIndex >= LED_ZONE_PIXEL_COUNT[LED_ZONE_Z1_MOUTH]) {
    return LED_COLOR_BLACK;
  }

  float brightness = active ?
    ledZ1MouthRenderActiveBrightness(localIndex) :
    ledZ1MouthRenderAmbientBrightness(localIndex);

  LedColor color = {
    0.0f,
    0.0f,
    brightness * LED_GLOBAL_BRIGHTNESS
  };

  return color;
}
