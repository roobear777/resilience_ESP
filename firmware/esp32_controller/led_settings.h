#ifndef TARDI_LED_SETTINGS_H
#define TARDI_LED_SETTINGS_H

#include <Arduino.h>

#include "led_config.h"

enum LedPaletteMode {
  LED_PALETTE_DEFAULT = 0,
  LED_PALETTE_WARM,
  LED_PALETTE_COOL,
  LED_PALETTE_EMBER,
  LED_PALETTE_OCEAN,
  LED_PALETTE_RAINBOW
};

enum LedBehaviorMode {
  LED_BEHAVIOR_NORMAL = 0,
  LED_BEHAVIOR_CALM,
  LED_BEHAVIOR_ENERGETIC,
  LED_BEHAVIOR_SPARKLE
};

enum LedLookKind {
  LED_LOOK_AMBIENT = 0,
  LED_LOOK_ANIMATION,
  LED_LOOK_COUNT
};

struct LedLookSettings {
  uint8_t brightness;
  uint8_t saturation;
  uint8_t speedPercent;
  LedPaletteMode paletteMode;
  LedBehaviorMode behaviorMode;
};

struct LedSettings {
  uint8_t masterBrightness;
  uint8_t saturationScale;
  uint8_t ambientLevel;
  uint8_t activeLevel;
  uint8_t speedPercent;
  uint16_t animationDurationSeconds;
  LedPaletteMode paletteMode;
  LedBehaviorMode behaviorMode;
  uint8_t zoneBrightness[LED_LOGICAL_ZONE_COUNT];
  LedLookSettings globalLook[LED_LOOK_COUNT];
  LedLookSettings zoneLook[LED_LOOK_COUNT][LED_LOGICAL_ZONE_COUNT];
};

void ledSettingsBegin();
bool ledSettingsLoadSaved();
bool ledSettingsSave();
void ledSettingsResetToDefaults();
bool ledSettingsResetSavedToDefaults();
const LedSettings& ledSettingsGet();
LedSettings& ledSettingsMutable();
uint8_t ledSettingsZoneBrightness(uint8_t zoneIndex);
uint32_t ledSettingsAnimationDurationMs();
uint16_t ledSettingsVersion();
bool ledSettingsLoadedFromSaved();
const LedLookSettings& ledSettingsGlobalLook(LedLookKind lookKind);
LedLookSettings& ledSettingsMutableGlobalLook(LedLookKind lookKind);
const LedLookSettings& ledSettingsZoneLook(LedLookKind lookKind, uint8_t zoneIndex);
LedLookSettings& ledSettingsMutableZoneLook(LedLookKind lookKind, uint8_t zoneIndex);
const char *ledSettingsPaletteName(LedPaletteMode mode);
const char *ledSettingsBehaviorName(LedBehaviorMode mode);
const char *ledSettingsLookKindName(LedLookKind lookKind);
bool ledSettingsParsePaletteName(const String &name, LedPaletteMode &mode);
bool ledSettingsParseBehaviorName(const String &name, LedBehaviorMode &mode);
bool ledSettingsParseLookKindName(const String &name, LedLookKind &lookKind);
void ledSettingsPrint(Stream &out);

#endif
