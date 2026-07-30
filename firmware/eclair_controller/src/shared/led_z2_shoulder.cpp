#include "led_z2_shoulder.h"

#include "led_animations.h"
#include "led_layout.h"

const float Z2_GLOBAL_BRIGHTNESS = 1.0f;

const float Z2_AMBIENT_HUE = 0.5f;
const float Z2_AMBIENT_SATURATION = 1.0f;
const float Z2_AMBIENT_BASE_BRIGHTNESS = 0.04f;
const float Z2_AMBIENT_PEAK_BRIGHTNESS = 0.14f;
const float Z2_AMBIENT_CYCLE_DURATION_MS = 3000.0f;

const float Z2_ACTIVE_HUE = 0.5f;
const float Z2_ACTIVE_SATURATION = 1.0f;
const float Z2_ACTIVE_BASE_BRIGHTNESS = 0.0f;
const float Z2_ACTIVE_PEAK_BRIGHTNESS = 0.95f;
const float Z2_ACTIVE_PEAK_MS = 1.0f;
const float Z2_ACTIVE_RAMP_DOWN_MS = 800.0f;
const float Z2_ACTIVE_STRIP_START_OFFSET_MS = 200.0f;
const float Z2_ACTIVE_REPEAT_INTERVAL_MS = 1000.0f;

static float z2AmbientElapsedMs = 0.0f;
static float z2ActiveCycle1ElapsedMs = -1.0f;
static float z2ActiveCycle2ElapsedMs = -1.0f;
static float z2ActiveRepeatTimerMs = 0.0f;

static uint8_t ledZ2ShoulderStripIndexFor(uint16_t localIndex) {
  uint16_t cumulativePixels = 0;

  for (uint8_t stripIndex = 0; stripIndex < LED_Z2_STRIP_COUNT; stripIndex++) {
    cumulativePixels += LED_Z2_STRIP_LENGTHS[stripIndex];

    if (localIndex < cumulativePixels) {
      return stripIndex;
    }
  }

  return LED_Z2_STRIP_COUNT - 1;
}

static float ledZ2ShoulderCycleBrightness(float cycleElapsedMs, uint8_t stripIndex) {
  if (cycleElapsedMs < 0.0f) {
    return -1.0f;
  }

  return ledPeakRampBrightness(
    cycleElapsedMs,
    stripIndex * Z2_ACTIVE_STRIP_START_OFFSET_MS,
    Z2_ACTIVE_PEAK_MS,
    0.0f,
    Z2_ACTIVE_RAMP_DOWN_MS,
    Z2_ACTIVE_BASE_BRIGHTNESS,
    Z2_ACTIVE_PEAK_BRIGHTNESS
  );
}

static LedColor ledZ2ShoulderRenderAmbient() {
  float breathAmount = ledTriangleWave(z2AmbientElapsedMs / Z2_AMBIENT_CYCLE_DURATION_MS);
  float brightness = Z2_AMBIENT_BASE_BRIGHTNESS +
    ((Z2_AMBIENT_PEAK_BRIGHTNESS - Z2_AMBIENT_BASE_BRIGHTNESS) * breathAmount);

  LedColor color = {
    Z2_AMBIENT_HUE,
    Z2_AMBIENT_SATURATION,
    brightness * Z2_GLOBAL_BRIGHTNESS
  };

  return color;
}

static LedColor ledZ2ShoulderRenderActive(uint16_t localIndex) {
  uint8_t stripIndex = ledZ2ShoulderStripIndexFor(localIndex);
  float cycle1Brightness = ledZ2ShoulderCycleBrightness(z2ActiveCycle1ElapsedMs, stripIndex);
  float cycle2Brightness = ledZ2ShoulderCycleBrightness(z2ActiveCycle2ElapsedMs, stripIndex);
  float brightness = Z2_ACTIVE_BASE_BRIGHTNESS;

  if (cycle1Brightness > brightness) {
    brightness = cycle1Brightness;
  }

  if (cycle2Brightness > brightness) {
    brightness = cycle2Brightness;
  }

  LedColor color = {
    Z2_ACTIVE_HUE,
    Z2_ACTIVE_SATURATION,
    brightness * Z2_GLOBAL_BRIGHTNESS
  };

  return color;
}

void ledZ2ShoulderBegin() {
  z2AmbientElapsedMs = 0.0f;
  z2ActiveCycle1ElapsedMs = -1.0f;
  z2ActiveCycle2ElapsedMs = -1.0f;
  z2ActiveRepeatTimerMs = 0.0f;
}

void ledZ2ShoulderUpdate(uint32_t deltaMs) {
  z2AmbientElapsedMs += deltaMs;

  while (z2AmbientElapsedMs >= Z2_AMBIENT_CYCLE_DURATION_MS) {
    z2AmbientElapsedMs -= Z2_AMBIENT_CYCLE_DURATION_MS;
  }

  float fullCycleDurationMs =
    ((LED_Z2_STRIP_COUNT - 1) * Z2_ACTIVE_STRIP_START_OFFSET_MS) +
    Z2_ACTIVE_PEAK_MS +
    Z2_ACTIVE_RAMP_DOWN_MS;

  if (z2ActiveCycle1ElapsedMs >= 0.0f) {
    z2ActiveCycle1ElapsedMs += deltaMs;

    if (z2ActiveCycle1ElapsedMs > fullCycleDurationMs) {
      z2ActiveCycle1ElapsedMs = -1.0f;
    }
  }

  if (z2ActiveCycle2ElapsedMs >= 0.0f) {
    z2ActiveCycle2ElapsedMs += deltaMs;

    if (z2ActiveCycle2ElapsedMs > fullCycleDurationMs) {
      z2ActiveCycle2ElapsedMs = -1.0f;
    }
  }

  z2ActiveRepeatTimerMs += deltaMs;

  if (z2ActiveRepeatTimerMs >= Z2_ACTIVE_REPEAT_INTERVAL_MS) {
    z2ActiveRepeatTimerMs = 0.0f;

    if (z2ActiveCycle1ElapsedMs < 0.0f) {
      z2ActiveCycle1ElapsedMs = 0.0f;
    } else if (z2ActiveCycle2ElapsedMs < 0.0f) {
      z2ActiveCycle2ElapsedMs = 0.0f;
    }
  }
}

LedColor ledZ2ShoulderRender(uint16_t localIndex, bool active, uint32_t nowMs) {
  (void)nowMs;

  if (localIndex >= LED_ZONE_PIXEL_COUNT[LED_ZONE_Z2_SHOULDER]) {
    return LED_COLOR_BLACK;
  }

  if (active) {
    return ledZ2ShoulderRenderActive(localIndex);
  }

  return ledZ2ShoulderRenderAmbient();
}
