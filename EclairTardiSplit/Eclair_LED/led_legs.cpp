#include "led_legs.h"

#include "led_animations.h"
#include "led_layout.h"

const float LEGS_GLOBAL_BRIGHTNESS = 1.0f;

const float LEGS_AMBIENT_HUE = 0.03f;
const float LEGS_AMBIENT_SATURATION = 1.0f;
const float LEGS_AMBIENT_BASE_BRIGHTNESS = 0.15f;
const float LEGS_AMBIENT_PULSE_PEAK_BRIGHTNESS = 0.35f;
const float LEGS_AMBIENT_PULSE_DURATION_MS = 800.0f;
const float LEGS_AMBIENT_CYCLE_DURATION_MS = 1600.0f;

const float LEGS_ACTIVE_BASE_HUE = 0.03f;
const float LEGS_ACTIVE_BASE_SATURATION = 1.0f;
const float LEGS_ACTIVE_BASE_BRIGHTNESS = 0.15f;
const float LEGS_ACTIVE_ZAP_HUE = 0.12f;
const float LEGS_ACTIVE_ZAP_SATURATION = 1.0f;
const float LEGS_ACTIVE_ZAP_BRIGHTNESS = 0.90f;
const float LEGS_ACTIVE_ZAP_LENGTH = 20.0f;
const float LEGS_ACTIVE_ZAP_DURATION_MS = 800.0f;
const float LEGS_ACTIVE_GROUP_DELAY_MS = 100.0f;
const float LEGS_ACTIVE_PAUSE_MS = 300.0f;

static float legsAmbientElapsedMs = 0.0f;
static float legsActiveElapsedMs = 0.0f;

static bool ledLegsResolveLogicalPosition(
  uint16_t localIndex,
  uint8_t outputZoneIndex,
  uint8_t *logicalLegIndex,
  uint8_t *ledInLeg
) {
  if (outputZoneIndex != LED_ZONE_Z5_FRONT_LEGS && outputZoneIndex != LED_ZONE_Z6_BACK_LEGS) {
    return false;
  }

  if (localIndex >= LED_ZONE_PIXEL_COUNT[outputZoneIndex]) {
    return false;
  }

  uint8_t legOffset = outputZoneIndex == LED_ZONE_Z6_BACK_LEGS ? LED_LEGS_PER_OUTPUT : 0;
  *logicalLegIndex = (localIndex / LED_LEG_PIXELS_PER_LEG) + legOffset;
  *ledInLeg = localIndex % LED_LEG_PIXELS_PER_LEG;

  return *logicalLegIndex < LED_LEG_LOGICAL_COUNT;
}

static LedColor ledLegsRenderAmbient(uint8_t logicalLegIndex) {
  bool isOddLeg = (logicalLegIndex % 2) != 0;
  float pulseStartMs = isOddLeg ? 0.0f : LEGS_AMBIENT_CYCLE_DURATION_MS / 2.0f;
  float timeSincePulseStartMs = legsAmbientElapsedMs - pulseStartMs;

  if (timeSincePulseStartMs < 0.0f) {
    timeSincePulseStartMs += LEGS_AMBIENT_CYCLE_DURATION_MS;
  }

  float brightness = LEGS_AMBIENT_BASE_BRIGHTNESS;

  if (timeSincePulseStartMs < LEGS_AMBIENT_PULSE_DURATION_MS) {
    float pulseAmount = ledTriangleWave(timeSincePulseStartMs / LEGS_AMBIENT_PULSE_DURATION_MS);
    brightness = LEGS_AMBIENT_BASE_BRIGHTNESS +
      ((LEGS_AMBIENT_PULSE_PEAK_BRIGHTNESS - LEGS_AMBIENT_BASE_BRIGHTNESS) * pulseAmount);
  }

  LedColor color = {
    LEGS_AMBIENT_HUE,
    LEGS_AMBIENT_SATURATION,
    brightness * LEGS_GLOBAL_BRIGHTNESS
  };

  return color;
}

static LedColor ledLegsRenderActive(uint8_t logicalLegIndex, uint8_t ledInLeg) {
  bool isOddLeg = (logicalLegIndex % 2) != 0;
  float legStartMs = isOddLeg ? 0.0f : LEGS_ACTIVE_GROUP_DELAY_MS;
  float legElapsedMs = legsActiveElapsedMs - legStartMs;
  float peakPosition =
    (legElapsedMs / LEGS_ACTIVE_ZAP_DURATION_MS) *
    (LED_LEG_PIXELS_PER_LEG + LEGS_ACTIVE_ZAP_LENGTH) -
    LEGS_ACTIVE_ZAP_LENGTH;
  float distanceBehind = peakPosition - ledInLeg;
  float hue = LEGS_ACTIVE_BASE_HUE;
  float saturation = LEGS_ACTIVE_BASE_SATURATION;
  float brightness = LEGS_ACTIVE_BASE_BRIGHTNESS;

  if (legElapsedMs >= 0.0f &&
      distanceBehind >= 0.0f &&
      distanceBehind <= LEGS_ACTIVE_ZAP_LENGTH) {
    float rampFraction = distanceBehind / LEGS_ACTIVE_ZAP_LENGTH;
    brightness = LEGS_ACTIVE_ZAP_BRIGHTNESS -
      ((LEGS_ACTIVE_ZAP_BRIGHTNESS - LEGS_ACTIVE_BASE_BRIGHTNESS) * rampFraction);
  }

  if (brightness > LEGS_ACTIVE_BASE_BRIGHTNESS) {
    float blendFraction =
      (brightness - LEGS_ACTIVE_BASE_BRIGHTNESS) /
      (LEGS_ACTIVE_ZAP_BRIGHTNESS - LEGS_ACTIVE_BASE_BRIGHTNESS);
    hue = LEGS_ACTIVE_BASE_HUE + ((LEGS_ACTIVE_ZAP_HUE - LEGS_ACTIVE_BASE_HUE) * blendFraction);
    saturation = LEGS_ACTIVE_ZAP_SATURATION;
  }

  LedColor color = {
    hue,
    saturation,
    brightness * LEGS_GLOBAL_BRIGHTNESS
  };

  return color;
}

void ledLegsBegin() {
  legsAmbientElapsedMs = 0.0f;
  legsActiveElapsedMs = 0.0f;
}

void ledLegsUpdate(uint32_t deltaMs) {
  legsAmbientElapsedMs += deltaMs;

  while (legsAmbientElapsedMs >= LEGS_AMBIENT_CYCLE_DURATION_MS) {
    legsAmbientElapsedMs -= LEGS_AMBIENT_CYCLE_DURATION_MS;
  }

  legsActiveElapsedMs += deltaMs;

  float activeCycleTotalMs =
    LEGS_ACTIVE_GROUP_DELAY_MS +
    LEGS_ACTIVE_ZAP_DURATION_MS +
    (LEGS_ACTIVE_ZAP_LENGTH * (LEGS_ACTIVE_ZAP_DURATION_MS / LED_LEG_PIXELS_PER_LEG)) +
    LEGS_ACTIVE_PAUSE_MS;

  while (legsActiveElapsedMs >= activeCycleTotalMs) {
    legsActiveElapsedMs -= activeCycleTotalMs;
  }
}

LedColor ledLegsRender(uint16_t localIndex, uint8_t outputZoneIndex, bool active, uint32_t nowMs) {
  (void)nowMs;

  uint8_t logicalLegIndex = 0;
  uint8_t ledInLeg = 0;

  if (!ledLegsResolveLogicalPosition(localIndex, outputZoneIndex, &logicalLegIndex, &ledInLeg)) {
    return LED_COLOR_BLACK;
  }

  if (active) {
    return ledLegsRenderActive(logicalLegIndex, ledInLeg);
  }

  return ledLegsRenderAmbient(logicalLegIndex);
}
