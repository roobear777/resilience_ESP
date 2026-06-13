#include "led_engine.h"

#include "led_config.h"
#include "led_legs.h"
#include "led_layout.h"
#include "led_state.h"
#include "led_z1_mouth.h"
#include "led_z2_shoulder.h"
#include "led_z3_midbody.h"
#include "led_z4_rear.h"
#include "led_z7_digestive.h"
#include "led_z8_stations.h"

static bool ledPressureTestEnabled = false;
static uint32_t ledLastFrameMs = 0;
static uint32_t ledFrameDeltaMs = 0;

void ledEngineBegin() {
  ledPressureTestEnabled = false;
  ledLastFrameMs = 0;
  ledFrameDeltaMs = 0;
  ledZ1MouthBegin();
  ledZ2ShoulderBegin();
  ledZ3MidbodyBegin();
  ledZ4RearBegin();
  ledLegsBegin();
  ledZ7DigestiveBegin();
  ledZ8StationsBegin();
}

void ledEngineUpdate(uint32_t nowMs) {
  if (ledLastFrameMs == 0) {
    ledFrameDeltaMs = 0;
  } else {
    ledFrameDeltaMs = nowMs - ledLastFrameMs;
  }

  ledLastFrameMs = nowMs;
  ledZ1MouthUpdate(ledFrameDeltaMs);
  ledZ2ShoulderUpdate(ledFrameDeltaMs);
  ledZ3MidbodyUpdate(ledFrameDeltaMs);
  ledZ4RearUpdate(ledFrameDeltaMs);
  ledLegsUpdate(ledFrameDeltaMs);
  ledZ7DigestiveUpdate(ledFrameDeltaMs);
  ledZ8StationsUpdate(ledFrameDeltaMs);
}

bool ledEngineIsPressureTestEnabled() {
  return ledPressureTestEnabled;
}

void ledEngineSetPressureTestEnabled(bool enabled) {
  ledPressureTestEnabled = enabled;
}

bool ledEngineIsZoneActive(uint8_t zoneIndex, uint32_t nowMs) {
  if (ledPressureTestEnabled) {
    return true;
  }

  return ledIsZoneActive(zoneIndex, nowMs);
}

LedColor ledEngineRenderPixel(uint16_t logicalPixelIndex, uint32_t nowMs) {
  if (logicalPixelIndex >= LED_TOTAL_PIXEL_COUNT) {
    return LED_COLOR_BLACK;
  }

  if (logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z1_MOUTH]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z1_MOUTH];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z1_MOUTH, nowMs);
    return ledZ1MouthRender(localIndex, active, nowMs);
  }

  if (logicalPixelIndex >= LED_ZONE_START[LED_ZONE_Z2_SHOULDER] &&
      logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z2_SHOULDER]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z2_SHOULDER];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z2_SHOULDER, nowMs);
    return ledZ2ShoulderRender(localIndex, active, nowMs);
  }

  if (logicalPixelIndex >= LED_ZONE_START[LED_ZONE_Z3_MIDBODY] &&
      logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z3_MIDBODY]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z3_MIDBODY];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z3_MIDBODY, nowMs);
    return ledZ3MidbodyRender(localIndex, active, nowMs);
  }

  if (logicalPixelIndex >= LED_ZONE_START[LED_ZONE_Z4_REAR] &&
      logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z4_REAR]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z4_REAR];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z4_REAR, nowMs);
    return ledZ4RearRender(localIndex, active, nowMs);
  }

  if (logicalPixelIndex >= LED_ZONE_START[LED_ZONE_Z5_FRONT_LEGS] &&
      logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z5_FRONT_LEGS]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z5_FRONT_LEGS];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z5_FRONT_LEGS, nowMs);
    return ledLegsRender(localIndex, LED_ZONE_Z5_FRONT_LEGS, active, nowMs);
  }

  if (logicalPixelIndex >= LED_ZONE_START[LED_ZONE_Z6_BACK_LEGS] &&
      logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z6_BACK_LEGS]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z6_BACK_LEGS];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z6_BACK_LEGS, nowMs);
    return ledLegsRender(localIndex, LED_ZONE_Z6_BACK_LEGS, active, nowMs);
  }

  if (logicalPixelIndex >= LED_ZONE_START[LED_ZONE_Z7_DIGESTIVE] &&
      logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z7_DIGESTIVE]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z7_DIGESTIVE];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z7_DIGESTIVE, nowMs);
    return ledZ7DigestiveRender(localIndex, active, nowMs);
  }

  if (logicalPixelIndex >= LED_ZONE_START[LED_ZONE_Z8_STATIONS] &&
      logicalPixelIndex <= LED_ZONE_END[LED_ZONE_Z8_STATIONS]) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z8_STATIONS];
    uint8_t station = ledZ8StationFor(localIndex);
    bool stationActive = ledEngineIsZoneActive(station, nowMs);
    return ledZ8StationsRender(localIndex, stationActive, nowMs);
  }

  return LED_COLOR_BLACK;
}
