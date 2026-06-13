#include "led_z8_stations.h"

#include "led_animations.h"
#include "led_layout.h"

const float Z8_GLOBAL_BRIGHTNESS = 1.0f;

const float Z8_HUE = 0.08f;
const float Z8_SATURATION = 0.85f;
const float Z8_AMBIENT_BASE_BRIGHTNESS = 0.06f;
const float Z8_AMBIENT_PEAK_BRIGHTNESS = 0.20f;
const float Z8_AMBIENT_CYCLE_DURATION_MS = 4000.0f;
const float Z8_ACTIVE_BASE_BRIGHTNESS = 0.10f;
const float Z8_ACTIVE_CHASE_BRIGHTNESS = 0.95f;
const float Z8_ACTIVE_CHASE_DURATION_MS = 700.0f;
const float Z8_ACTIVE_TAIL_LENGTH = 5.0f;

static float z8AmbientElapsedMs = 0.0f;
static float z8ActiveElapsedMs = 0.0f;

static LedColor ledZ8StationsColor(float brightness) {
  LedColor color = {
    Z8_HUE,
    Z8_SATURATION,
    brightness * Z8_GLOBAL_BRIGHTNESS
  };

  return color;
}

static LedColor ledZ8StationsRenderAmbient(uint8_t station) {
  float progress =
    (z8AmbientElapsedMs / Z8_AMBIENT_CYCLE_DURATION_MS) +
    (float(station) / LED_Z8_STATION_STRING_COUNT);
  float breathAmount = ledTriangleWave(progress);
  float brightness = Z8_AMBIENT_BASE_BRIGHTNESS +
    ((Z8_AMBIENT_PEAK_BRIGHTNESS - Z8_AMBIENT_BASE_BRIGHTNESS) * breathAmount);

  return ledZ8StationsColor(brightness);
}

static LedColor ledZ8StationsRenderActive(uint8_t posInString) {
  float headPosition =
    (z8ActiveElapsedMs / Z8_ACTIVE_CHASE_DURATION_MS) *
    (LED_Z8_PIXELS_PER_STATION_STRING + Z8_ACTIVE_TAIL_LENGTH);
  float distanceBehind = headPosition - posInString;
  float brightness = Z8_ACTIVE_BASE_BRIGHTNESS;

  if (distanceBehind >= 0.0f && distanceBehind <= Z8_ACTIVE_TAIL_LENGTH) {
    float rampFraction = distanceBehind / Z8_ACTIVE_TAIL_LENGTH;
    brightness = Z8_ACTIVE_CHASE_BRIGHTNESS -
      ((Z8_ACTIVE_CHASE_BRIGHTNESS - Z8_ACTIVE_BASE_BRIGHTNESS) * rampFraction);
  }

  return ledZ8StationsColor(brightness);
}

void ledZ8StationsBegin() {
  z8AmbientElapsedMs = 0.0f;
  z8ActiveElapsedMs = 0.0f;
}

void ledZ8StationsUpdate(uint32_t deltaMs) {
  z8AmbientElapsedMs += deltaMs;

  while (z8AmbientElapsedMs >= Z8_AMBIENT_CYCLE_DURATION_MS) {
    z8AmbientElapsedMs -= Z8_AMBIENT_CYCLE_DURATION_MS;
  }

  z8ActiveElapsedMs += deltaMs;

  while (z8ActiveElapsedMs >= Z8_ACTIVE_CHASE_DURATION_MS) {
    z8ActiveElapsedMs -= Z8_ACTIVE_CHASE_DURATION_MS;
  }
}

uint8_t ledZ8StationFor(uint16_t localIndex) {
  uint8_t station = localIndex / LED_Z8_PIXELS_PER_STATION_STRING;

  if (station >= LED_Z8_STATION_STRING_COUNT) {
    return LED_Z8_STATION_STRING_COUNT - 1;
  }

  return station;
}

LedColor ledZ8StationsRender(uint16_t localIndex, bool stationActive, uint32_t nowMs) {
  (void)nowMs;

  if (localIndex >= LED_ZONE_PIXEL_COUNT[LED_ZONE_Z8_STATIONS]) {
    return LED_COLOR_BLACK;
  }

  uint8_t station = ledZ8StationFor(localIndex);
  uint8_t posInString = localIndex % LED_Z8_PIXELS_PER_STATION_STRING;

  if (stationActive) {
    return ledZ8StationsRenderActive(posInString);
  }

  return ledZ8StationsRenderAmbient(station);
}
