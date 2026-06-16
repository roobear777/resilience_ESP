#include "led_engine.h"

#include "led_config.h"
#include "led_legs.h"
#include "led_layout.h"
#include "led_settings.h"
#include "led_state.h"
#include "led_z1_mouth.h"
#include "led_z2_shoulder.h"
#include "led_z3_midbody.h"
#include "led_z4_rear.h"
#include "led_z7_digestive.h"
#include "led_z8_stations.h"

#include <math.h>

static bool ledPressureTestEnabled = false;
static uint32_t ledLastFrameMs = 0;
static uint32_t ledFrameDeltaMs = 0;

static float ledClamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }

  if (value > 1.0f) {
    return 1.0f;
  }

  return value;
}

static float ledByteScale(uint8_t value) {
  return static_cast<float>(value) / 255.0f;
}

static float ledWrap01(float value) {
  float wrapped = value - floorf(value);

  if (wrapped < 0.0f) {
    wrapped += 1.0f;
  }

  return wrapped;
}

static uint32_t ledScaledDeltaMs(uint32_t deltaMs) {
  const LedLookSettings &look = ledSettingsGlobalLook(LED_LOOK_ANIMATION);
  return static_cast<uint32_t>(
    (static_cast<uint64_t>(deltaMs) * look.speedPercent) / 100u
  );
}

static uint32_t ledScaledNowMs(
  uint32_t nowMs,
  LedLookKind lookKind,
  uint8_t zoneIndex
) {
  const LedLookSettings &globalLook = ledSettingsGlobalLook(lookKind);
  const LedLookSettings &zoneLook = ledSettingsZoneLook(lookKind, zoneIndex);
  uint16_t speedPercent = (
    static_cast<uint16_t>(globalLook.speedPercent)
    * static_cast<uint16_t>(zoneLook.speedPercent)
    + 50u
  ) / 100u;

  return static_cast<uint32_t>(
    (static_cast<uint64_t>(nowMs) * speedPercent) / 100u
  );
}

static LedColor ledApplyPaletteMode(
  const LedColor &color,
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  const LedLookSettings &globalLook,
  const LedLookSettings &zoneLook
) {
  LedColor tuned = color;
  LedPaletteMode paletteMode = zoneLook.paletteMode == LED_PALETTE_DEFAULT
    ? globalLook.paletteMode
    : zoneLook.paletteMode;

  switch (paletteMode) {
    case LED_PALETTE_WARM:
      tuned.h = ledWrap01((tuned.h * 0.55f) + 0.05f);
      tuned.s = ledClamp01(tuned.s * 0.9f + 0.1f);
      break;
    case LED_PALETTE_COOL:
      tuned.h = ledWrap01((tuned.h * 0.45f) + 0.52f);
      tuned.s = ledClamp01(tuned.s * 0.95f + 0.05f);
      break;
    case LED_PALETTE_EMBER:
      tuned.h = ledWrap01(0.03f + (tuned.h * 0.08f));
      tuned.s = ledClamp01(tuned.s * 0.8f + 0.2f);
      break;
    case LED_PALETTE_OCEAN:
      tuned.h = ledWrap01(0.50f + (tuned.h * 0.15f));
      tuned.s = ledClamp01(tuned.s * 0.9f + 0.1f);
      break;
    case LED_PALETTE_RAINBOW:
      tuned.h = ledWrap01(
        tuned.h
        + (static_cast<float>((nowMs / 50u) % 1000u) / 1000.0f)
        + (static_cast<float>(logicalPixelIndex % 97u) / 97.0f)
      );
      tuned.s = ledClamp01(tuned.s * 0.85f + 0.15f);
      break;
    case LED_PALETTE_DEFAULT:
    default:
      break;
  }

  return tuned;
}

static LedColor ledApplyBehaviorMode(
  const LedColor &color,
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  const LedLookSettings &globalLook,
  const LedLookSettings &zoneLook
) {
  LedColor tuned = color;
  LedBehaviorMode behaviorMode = zoneLook.behaviorMode == LED_BEHAVIOR_NORMAL
    ? globalLook.behaviorMode
    : zoneLook.behaviorMode;

  switch (behaviorMode) {
    case LED_BEHAVIOR_CALM:
      tuned.s = ledClamp01(tuned.s * 0.82f);
      tuned.v = ledClamp01(tuned.v * 0.75f);
      break;
    case LED_BEHAVIOR_ENERGETIC:
      tuned.s = ledClamp01(tuned.s * 1.12f);
      tuned.v = ledClamp01(tuned.v * 1.15f);
      break;
    case LED_BEHAVIOR_SPARKLE: {
      uint16_t sparklePhase = static_cast<uint16_t>(
        (logicalPixelIndex * 37u + (nowMs / 90u)) % 17u
      );
      float sparkle = sparklePhase == 0 ? 1.35f : 0.92f;
      tuned.s = ledClamp01(tuned.s * 1.08f);
      tuned.v = ledClamp01(tuned.v * sparkle);
      break;
    }
    case LED_BEHAVIOR_NORMAL:
    default:
      break;
  }

  return tuned;
}

static LedColor ledApplySettingsToColor(
  const LedColor &color,
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  uint8_t zoneIndex,
  bool active
) {
  LedLookKind lookKind = active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT;
  const LedLookSettings &globalLook = ledSettingsGlobalLook(lookKind);
  const LedLookSettings &zoneLook = ledSettingsZoneLook(lookKind, zoneIndex);
  const LedSettings &legacySettings = ledSettingsGet();
  const uint8_t zoneBrightness = ledSettingsZoneBrightness(zoneIndex);
  const uint8_t activityLevel = active ? legacySettings.activeLevel : legacySettings.ambientLevel;

  LedColor tuned = color;
  tuned = ledApplyPaletteMode(tuned, logicalPixelIndex, nowMs, globalLook, zoneLook);
  tuned.s = ledClamp01(
    tuned.s
    * ledByteScale(legacySettings.saturationScale)
    * ledByteScale(globalLook.saturation)
    * ledByteScale(zoneLook.saturation)
  );
  tuned.v = ledClamp01(
    tuned.v
    * ledByteScale(legacySettings.masterBrightness)
    * ledByteScale(zoneBrightness)
    * ledByteScale(activityLevel)
    * ledByteScale(globalLook.brightness)
    * ledByteScale(zoneLook.brightness)
  );
  tuned = ledApplyBehaviorMode(tuned, logicalPixelIndex, nowMs, globalLook, zoneLook);
  return tuned;
}

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

  uint32_t scaledDeltaMs = ledScaledDeltaMs(ledFrameDeltaMs);
  ledZ1MouthUpdate(scaledDeltaMs);
  ledZ2ShoulderUpdate(scaledDeltaMs);
  ledZ3MidbodyUpdate(scaledDeltaMs);
  ledZ4RearUpdate(scaledDeltaMs);
  ledLegsUpdate(scaledDeltaMs);
  ledZ7DigestiveUpdate(scaledDeltaMs);
  ledZ8StationsUpdate(scaledDeltaMs);
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
  uint8_t zoneIndex = 0;

  if (!ledLayoutZoneForPixel(logicalPixelIndex, zoneIndex)) {
    return LED_COLOR_BLACK;
  }

  if (zoneIndex == LED_ZONE_Z1_MOUTH) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z1_MOUTH];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z1_MOUTH, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledZ1MouthRender(localIndex, active, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, active);
  }

  if (zoneIndex == LED_ZONE_Z2_SHOULDER) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z2_SHOULDER];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z2_SHOULDER, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledZ2ShoulderRender(localIndex, active, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, active);
  }

  if (zoneIndex == LED_ZONE_Z3_MIDBODY) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z3_MIDBODY];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z3_MIDBODY, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledZ3MidbodyRender(localIndex, active, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, active);
  }

  if (zoneIndex == LED_ZONE_Z4_REAR) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z4_REAR];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z4_REAR, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledZ4RearRender(localIndex, active, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, active);
  }

  if (zoneIndex == LED_ZONE_Z5_FRONT_LEGS) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z5_FRONT_LEGS];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z5_FRONT_LEGS, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledLegsRender(localIndex, LED_ZONE_Z5_FRONT_LEGS, active, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, active);
  }

  if (zoneIndex == LED_ZONE_Z6_BACK_LEGS) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z6_BACK_LEGS];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z6_BACK_LEGS, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledLegsRender(localIndex, LED_ZONE_Z6_BACK_LEGS, active, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, active);
  }

  if (zoneIndex == LED_ZONE_Z7_DIGESTIVE) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z7_DIGESTIVE];
    bool active = ledEngineIsZoneActive(LED_ZONE_Z7_DIGESTIVE, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, active ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledZ7DigestiveRender(localIndex, active, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, active);
  }

  if (zoneIndex == LED_ZONE_Z8_STATIONS) {
    uint16_t localIndex = logicalPixelIndex - LED_ZONE_START[LED_ZONE_Z8_STATIONS];
    uint8_t station = ledZ8StationFor(localIndex);
    bool stationActive = ledEngineIsZoneActive(station, nowMs);
    uint32_t renderNowMs = ledScaledNowMs(nowMs, stationActive ? LED_LOOK_ANIMATION : LED_LOOK_AMBIENT, zoneIndex);
    LedColor color = ledZ8StationsRender(localIndex, stationActive, renderNowMs);
    return ledApplySettingsToColor(color, logicalPixelIndex, renderNowMs, zoneIndex, stationActive);
  }

  return LED_COLOR_BLACK;
}
