#include "led_z3_midbody.h"

#include <math.h>

#include "led_animations.h"
#include "led_layout.h"

const float Z3_GLOBAL_BRIGHTNESS = 1.0f;

const float Z3_AMBIENT_HUE = 0.67f;
const float Z3_AMBIENT_SATURATION = 1.0f;
const float Z3_AMBIENT_BASE_BRIGHTNESS = 0.04f;
const float Z3_AMBIENT_PEAK_BRIGHTNESS = 0.12f;
const float Z3_AMBIENT_CYCLE_DURATION_MS = 3300.0f;

const float Z3_ACTIVE_HUE = 0.67f;
const float Z3_ACTIVE_SATURATION = 1.0f;
const float Z3_ACTIVE_AMBIENT_BRIGHTNESS = 0.08f;
const float Z3_ACTIVE_PEAK_BRIGHTNESS = 0.85f;
const float Z3_ACTIVE_STROBE_HZ = 12.0f;
const uint8_t Z3_ACTIVE_STROBE_CYCLES = 8;
const float Z3_ACTIVE_PAIR_DELAY_MS = 120.0f;
const float Z3_ACTIVE_REPEAT_INTERVAL_MS = 1000.0f;

static float z3AmbientElapsedMs = 0.0f;
static float z3ActiveStrobeHalfCycleMs = 0.0f;
static float z3ActivePairTotalMs = 0.0f;
static bool z3ActiveWaveActive = false;
static float z3ActiveWaveElapsedMs = 0.0f;
static float z3ActiveRepeatTimerMs = 0.0f;

static uint8_t ledZ3MidbodyDistanceFromCenter(uint8_t ringIndex) {
  if (ringIndex == 3 || ringIndex == 4) {
    return 0;
  }

  if (ringIndex == 2 || ringIndex == 5) {
    return 1;
  }

  if (ringIndex == 1 || ringIndex == 6) {
    return 2;
  }

  return 3;
}

static LedColor ledZ3MidbodyColor(float brightness) {
  LedColor color = {
    Z3_ACTIVE_HUE,
    Z3_ACTIVE_SATURATION,
    brightness * Z3_GLOBAL_BRIGHTNESS
  };

  return color;
}

static LedColor ledZ3MidbodyRenderAmbient() {
  float breathAmount = ledTriangleWave(z3AmbientElapsedMs / Z3_AMBIENT_CYCLE_DURATION_MS);
  float brightness = Z3_AMBIENT_BASE_BRIGHTNESS +
    ((Z3_AMBIENT_PEAK_BRIGHTNESS - Z3_AMBIENT_BASE_BRIGHTNESS) * breathAmount);

  LedColor color = {
    Z3_AMBIENT_HUE,
    Z3_AMBIENT_SATURATION,
    brightness * Z3_GLOBAL_BRIGHTNESS
  };

  return color;
}

static LedColor ledZ3MidbodyRenderActive(uint16_t localIndex) {
  if (!z3ActiveWaveActive) {
    return ledZ3MidbodyColor(Z3_ACTIVE_AMBIENT_BRIGHTNESS);
  }

  uint8_t ringIndex = localIndex / LED_Z3_PIXELS_PER_RING;
  uint8_t distanceFromCenter = ledZ3MidbodyDistanceFromCenter(ringIndex);
  float pairStartMs = distanceFromCenter * Z3_ACTIVE_PAIR_DELAY_MS;
  float pairElapsedMs = z3ActiveWaveElapsedMs - pairStartMs;

  if (pairElapsedMs < 0.0f || pairElapsedMs >= z3ActivePairTotalMs) {
    return ledZ3MidbodyColor(Z3_ACTIVE_AMBIENT_BRIGHTNESS);
  }

  float fullCycleMs = 2.0f * z3ActiveStrobeHalfCycleMs;
  uint8_t cycleNumber = floorf(pairElapsedMs / fullCycleMs);
  float withinCycleMs = pairElapsedMs - (cycleNumber * fullCycleMs);

  if (withinCycleMs < z3ActiveStrobeHalfCycleMs) {
    float rampFraction = cycleNumber / float(Z3_ACTIVE_STROBE_CYCLES - 1);
    float onBrightness = Z3_ACTIVE_PEAK_BRIGHTNESS -
      ((Z3_ACTIVE_PEAK_BRIGHTNESS - Z3_ACTIVE_AMBIENT_BRIGHTNESS) * rampFraction);

    return ledZ3MidbodyColor(onBrightness);
  }

  return ledZ3MidbodyColor(Z3_ACTIVE_AMBIENT_BRIGHTNESS);
}

void ledZ3MidbodyBegin() {
  z3AmbientElapsedMs = 0.0f;
  z3ActiveStrobeHalfCycleMs = 500.0f / Z3_ACTIVE_STROBE_HZ;
  z3ActivePairTotalMs = Z3_ACTIVE_STROBE_CYCLES * 2.0f * z3ActiveStrobeHalfCycleMs;
  z3ActiveWaveActive = false;
  z3ActiveWaveElapsedMs = 0.0f;
  z3ActiveRepeatTimerMs = 0.0f;
}

void ledZ3MidbodyUpdate(uint32_t deltaMs) {
  z3AmbientElapsedMs += deltaMs;

  while (z3AmbientElapsedMs >= Z3_AMBIENT_CYCLE_DURATION_MS) {
    z3AmbientElapsedMs -= Z3_AMBIENT_CYCLE_DURATION_MS;
  }

  z3ActiveStrobeHalfCycleMs = 500.0f / Z3_ACTIVE_STROBE_HZ;
  z3ActivePairTotalMs = Z3_ACTIVE_STROBE_CYCLES * 2.0f * z3ActiveStrobeHalfCycleMs;

  if (!z3ActiveWaveActive) {
    z3ActiveRepeatTimerMs += deltaMs;

    if (z3ActiveRepeatTimerMs >= Z3_ACTIVE_REPEAT_INTERVAL_MS) {
      z3ActiveWaveActive = true;
      z3ActiveWaveElapsedMs = 0.0f;
      z3ActiveRepeatTimerMs = 0.0f;
    }

    return;
  }

  z3ActiveWaveElapsedMs += deltaMs;

  float waveTotalMs = 3.0f * Z3_ACTIVE_PAIR_DELAY_MS + z3ActivePairTotalMs;

  if (z3ActiveWaveElapsedMs >= waveTotalMs) {
    z3ActiveWaveActive = false;
    z3ActiveWaveElapsedMs = 0.0f;
  }
}

LedColor ledZ3MidbodyRender(uint16_t localIndex, bool active, uint32_t nowMs) {
  (void)nowMs;

  if (localIndex >= LED_ZONE_PIXEL_COUNT[LED_ZONE_Z3_MIDBODY]) {
    return LED_COLOR_BLACK;
  }

  if (active) {
    return ledZ3MidbodyRenderActive(localIndex);
  }

  return ledZ3MidbodyRenderAmbient();
}
