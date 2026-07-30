#include "led_z4_rear.h"

#include <math.h>

#include "led_animations.h"
#include "led_layout.h"

const float Z4_GLOBAL_BRIGHTNESS = 1.0f;

const float Z4_AMBIENT_HUE = 0.917f;
const float Z4_AMBIENT_SATURATION = 1.0f;
const float Z4_AMBIENT_BASE_BRIGHTNESS = 0.03f;
const float Z4_AMBIENT_PEAK_BRIGHTNESS = 0.10f;
const float Z4_AMBIENT_CYCLE_DURATION_MS = 4000.0f;

const float Z4_ACTIVE_HUE = 0.917f;
const float Z4_ACTIVE_SATURATION = 1.0f;
const float Z4_ACTIVE_AMBIENT_BRIGHTNESS = 0.02f;
const float Z4_ACTIVE_PEAK_BRIGHTNESS = 1.0f;
const float Z4_ACTIVE_GRADIENT_WIDTH = 8.0f;
const float Z4_ACTIVE_FALL_SPEED_MS = 500.0f;
const float Z4_ACTIVE_STRIP_DELAY_MS = 50.0f;
const float Z4_ACTIVE_REPEAT_INTERVAL_MS = 1500.0f;

static float z4AmbientElapsedMs = 0.0f;
static float z4ActiveCycle1ElapsedMs = -1.0f;
static float z4ActiveCycle2ElapsedMs = -1.0f;
static float z4ActiveRepeatTimerMs = 0.0f;
static uint8_t z4ActiveCenterA = 5;
static uint8_t z4ActiveCenterB = 6;
static uint8_t z4ActiveMaxDistance = 5;
static float z4ActiveFullFallMs = 0.0f;

static void ledZ4RearUpdateDerivedActiveState() {
  z4ActiveCenterA = (LED_Z4_STRIP_COUNT - 1) / 2;
  z4ActiveCenterB = LED_Z4_STRIP_COUNT / 2;
  z4ActiveMaxDistance = z4ActiveCenterA;
  z4ActiveFullFallMs =
    Z4_ACTIVE_FALL_SPEED_MS *
    (LED_Z4_PIXELS_PER_STRIP + Z4_ACTIVE_GRADIENT_WIDTH) /
    LED_Z4_PIXELS_PER_STRIP;
}

static uint8_t ledZ4RearDistanceFromCenter(uint8_t stripIndex) {
  if (stripIndex <= z4ActiveCenterA) {
    return z4ActiveCenterA - stripIndex;
  }

  return stripIndex - z4ActiveCenterB;
}

static float ledZ4RearCycleBrightness(float cycleElapsedMs, uint8_t stripIndex, uint8_t ledInStrip) {
  if (cycleElapsedMs < 0.0f) {
    return -1.0f;
  }

  uint8_t distanceFromCenter = ledZ4RearDistanceFromCenter(stripIndex);
  float stripStartMs = distanceFromCenter * Z4_ACTIVE_STRIP_DELAY_MS;
  float stripElapsedMs = cycleElapsedMs - stripStartMs;

  if (stripElapsedMs < 0.0f) {
    return -1.0f;
  }

  int peakLed = floorf((stripElapsedMs / Z4_ACTIVE_FALL_SPEED_MS) * LED_Z4_PIXELS_PER_STRIP);

  if (peakLed > LED_Z4_PIXELS_PER_STRIP + Z4_ACTIVE_GRADIENT_WIDTH) {
    return -1.0f;
  }

  if (ledInStrip == peakLed) {
    return Z4_ACTIVE_PEAK_BRIGHTNESS;
  }

  int distanceBehind = peakLed - ledInStrip;

  if (distanceBehind > 0 && distanceBehind <= Z4_ACTIVE_GRADIENT_WIDTH) {
    float rampFraction = distanceBehind / Z4_ACTIVE_GRADIENT_WIDTH;
    return Z4_ACTIVE_PEAK_BRIGHTNESS -
      ((Z4_ACTIVE_PEAK_BRIGHTNESS - Z4_ACTIVE_AMBIENT_BRIGHTNESS) * rampFraction);
  }

  return -1.0f;
}

static LedColor ledZ4RearColor(float brightness) {
  LedColor color = {
    Z4_ACTIVE_HUE,
    Z4_ACTIVE_SATURATION,
    brightness * Z4_GLOBAL_BRIGHTNESS
  };

  return color;
}

static LedColor ledZ4RearRenderAmbient() {
  float breathAmount = ledTriangleWave(z4AmbientElapsedMs / Z4_AMBIENT_CYCLE_DURATION_MS);
  float brightness = Z4_AMBIENT_BASE_BRIGHTNESS +
    ((Z4_AMBIENT_PEAK_BRIGHTNESS - Z4_AMBIENT_BASE_BRIGHTNESS) * breathAmount);

  LedColor color = {
    Z4_AMBIENT_HUE,
    Z4_AMBIENT_SATURATION,
    brightness * Z4_GLOBAL_BRIGHTNESS
  };

  return color;
}

static LedColor ledZ4RearRenderActive(uint16_t localIndex) {
  uint8_t stripIndex = localIndex / LED_Z4_PIXELS_PER_STRIP;
  uint8_t ledInStrip = localIndex % LED_Z4_PIXELS_PER_STRIP;
  float cycle1Brightness = ledZ4RearCycleBrightness(z4ActiveCycle1ElapsedMs, stripIndex, ledInStrip);
  float cycle2Brightness = ledZ4RearCycleBrightness(z4ActiveCycle2ElapsedMs, stripIndex, ledInStrip);
  float brightness = Z4_ACTIVE_AMBIENT_BRIGHTNESS;

  if (cycle1Brightness > brightness) {
    brightness = cycle1Brightness;
  }

  if (cycle2Brightness > brightness) {
    brightness = cycle2Brightness;
  }

  return ledZ4RearColor(brightness);
}

void ledZ4RearBegin() {
  z4AmbientElapsedMs = 0.0f;
  z4ActiveCycle1ElapsedMs = -1.0f;
  z4ActiveCycle2ElapsedMs = -1.0f;
  z4ActiveRepeatTimerMs = 0.0f;
  ledZ4RearUpdateDerivedActiveState();
}

void ledZ4RearUpdate(uint32_t deltaMs) {
  z4AmbientElapsedMs += deltaMs;

  while (z4AmbientElapsedMs >= Z4_AMBIENT_CYCLE_DURATION_MS) {
    z4AmbientElapsedMs -= Z4_AMBIENT_CYCLE_DURATION_MS;
  }

  ledZ4RearUpdateDerivedActiveState();

  float oneCycleTotalMs = z4ActiveMaxDistance * Z4_ACTIVE_STRIP_DELAY_MS + z4ActiveFullFallMs;

  if (z4ActiveCycle1ElapsedMs >= 0.0f) {
    z4ActiveCycle1ElapsedMs += deltaMs;

    if (z4ActiveCycle1ElapsedMs > oneCycleTotalMs) {
      z4ActiveCycle1ElapsedMs = -1.0f;
    }
  }

  if (z4ActiveCycle2ElapsedMs >= 0.0f) {
    z4ActiveCycle2ElapsedMs += deltaMs;

    if (z4ActiveCycle2ElapsedMs > oneCycleTotalMs) {
      z4ActiveCycle2ElapsedMs = -1.0f;
    }
  }

  z4ActiveRepeatTimerMs += deltaMs;

  if (z4ActiveRepeatTimerMs >= Z4_ACTIVE_REPEAT_INTERVAL_MS) {
    z4ActiveRepeatTimerMs = 0.0f;

    if (z4ActiveCycle1ElapsedMs < 0.0f) {
      z4ActiveCycle1ElapsedMs = 0.0f;
    } else if (z4ActiveCycle2ElapsedMs < 0.0f) {
      z4ActiveCycle2ElapsedMs = 0.0f;
    }
  }
}

LedColor ledZ4RearRender(uint16_t localIndex, bool active, uint32_t nowMs) {
  (void)nowMs;

  if (localIndex >= LED_ZONE_PIXEL_COUNT[LED_ZONE_Z4_REAR]) {
    return LED_COLOR_BLACK;
  }

  if (active) {
    return ledZ4RearRenderActive(localIndex);
  }

  return ledZ4RearRenderAmbient();
}
