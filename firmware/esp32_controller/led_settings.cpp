#include "led_settings.h"

#include <Preferences.h>

const uint16_t LED_SETTINGS_VERSION = 3;

static const char *LED_SETTINGS_NAMESPACE = "tardi-led";
static const char *LED_SETTINGS_KEY_VERSION = "version";
static const char *LED_SETTINGS_KEY_MASTER = "master";
static const char *LED_SETTINGS_KEY_SATURATION = "sat";
static const char *LED_SETTINGS_KEY_AMBIENT = "ambient";
static const char *LED_SETTINGS_KEY_ACTIVE = "active";
static const char *LED_SETTINGS_KEY_SPEED = "speed";
static const char *LED_SETTINGS_KEY_PALETTE = "palette";
static const char *LED_SETTINGS_KEY_BEHAVIOR = "behavior";

static LedSettings ledSettings;
static bool ledSettingsLoadedSaved = false;

static uint8_t ledSettingsClampedByte(uint16_t value) {
  if (value > 255) {
    return 255;
  }

  return static_cast<uint8_t>(value);
}

static String ledSettingsZoneKey(uint8_t zoneIndex) {
  String key = "zone";
  key += String(zoneIndex);
  return key;
}

static String ledSettingsLookKey(const char *prefix, LedLookKind lookKind, int8_t zoneIndex, const char *field) {
  String key = prefix;
  key += String(static_cast<uint8_t>(lookKind));

  if (zoneIndex >= 0) {
    key += String(zoneIndex);
  }

  key += field;
  return key;
}

static void ledSettingsResetLook(LedLookSettings &look) {
  look.brightness = 255;
  look.saturation = 255;
  look.speedPercent = 100;
  look.paletteMode = LED_PALETTE_DEFAULT;
  look.behaviorMode = LED_BEHAVIOR_NORMAL;
}

static void ledSettingsLoadLook(
  Preferences &preferences,
  const char *prefix,
  LedLookKind lookKind,
  int8_t zoneIndex,
  LedLookSettings &look
) {
  look.brightness = ledSettingsClampedByte(
    preferences.getUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "b").c_str(), 255)
  );
  look.saturation = ledSettingsClampedByte(
    preferences.getUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "s").c_str(), 255)
  );
  look.speedPercent = ledSettingsClampedByte(
    preferences.getUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "p").c_str(), 100)
  );
  look.paletteMode = static_cast<LedPaletteMode>(
    preferences.getUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "l").c_str(), LED_PALETTE_DEFAULT)
  );
  look.behaviorMode = static_cast<LedBehaviorMode>(
    preferences.getUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "m").c_str(), LED_BEHAVIOR_NORMAL)
  );

  if (look.paletteMode > LED_PALETTE_RAINBOW) {
    look.paletteMode = LED_PALETTE_DEFAULT;
  }

  if (look.behaviorMode > LED_BEHAVIOR_SPARKLE) {
    look.behaviorMode = LED_BEHAVIOR_NORMAL;
  }
}

static bool ledSettingsSaveLook(
  Preferences &preferences,
  const char *prefix,
  LedLookKind lookKind,
  int8_t zoneIndex,
  const LedLookSettings &look
) {
  bool ok = true;
  ok = preferences.putUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "b").c_str(), look.brightness) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "s").c_str(), look.saturation) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "p").c_str(), look.speedPercent) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "l").c_str(), static_cast<uint8_t>(look.paletteMode)) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(ledSettingsLookKey(prefix, lookKind, zoneIndex, "m").c_str(), static_cast<uint8_t>(look.behaviorMode)) == sizeof(uint8_t) && ok;
  return ok;
}

void ledSettingsBegin() {
  ledSettingsResetToDefaults();
  ledSettingsLoadSaved();
}

bool ledSettingsLoadSaved() {
  Preferences preferences;

  if (!preferences.begin(LED_SETTINGS_NAMESPACE, true)) {
    ledSettingsResetToDefaults();
    ledSettingsLoadedSaved = false;
    return false;
  }

  if (!preferences.isKey(LED_SETTINGS_KEY_VERSION)) {
    preferences.end();
    ledSettingsResetToDefaults();
    ledSettingsLoadedSaved = false;
    return false;
  }

  uint16_t savedVersion = preferences.getUShort(LED_SETTINGS_KEY_VERSION, 0);

  if (savedVersion != LED_SETTINGS_VERSION) {
    preferences.end();
    ledSettingsResetToDefaults();
    ledSettingsLoadedSaved = false;
    return false;
  }

  ledSettings.masterBrightness = ledSettingsClampedByte(
    preferences.getUChar(LED_SETTINGS_KEY_MASTER, 255)
  );
  ledSettings.saturationScale = ledSettingsClampedByte(
    preferences.getUChar(LED_SETTINGS_KEY_SATURATION, 255)
  );
  ledSettings.ambientLevel = ledSettingsClampedByte(
    preferences.getUChar(LED_SETTINGS_KEY_AMBIENT, 255)
  );
  ledSettings.activeLevel = ledSettingsClampedByte(
    preferences.getUChar(LED_SETTINGS_KEY_ACTIVE, 255)
  );
  ledSettings.speedPercent = ledSettingsClampedByte(
    preferences.getUChar(LED_SETTINGS_KEY_SPEED, 100)
  );
  ledSettings.paletteMode = static_cast<LedPaletteMode>(
    preferences.getUChar(LED_SETTINGS_KEY_PALETTE, LED_PALETTE_DEFAULT)
  );
  ledSettings.behaviorMode = static_cast<LedBehaviorMode>(
    preferences.getUChar(LED_SETTINGS_KEY_BEHAVIOR, LED_BEHAVIOR_NORMAL)
  );

  if (ledSettings.paletteMode > LED_PALETTE_RAINBOW) {
    ledSettings.paletteMode = LED_PALETTE_DEFAULT;
  }

  if (ledSettings.behaviorMode > LED_BEHAVIOR_SPARKLE) {
    ledSettings.behaviorMode = LED_BEHAVIOR_NORMAL;
  }

  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    String key = ledSettingsZoneKey(i);
    ledSettings.zoneBrightness[i] = ledSettingsClampedByte(
      preferences.getUChar(key.c_str(), 255)
    );
  }

  for (uint8_t look = 0; look < LED_LOOK_COUNT; look++) {
    LedLookKind lookKind = static_cast<LedLookKind>(look);
    ledSettingsLoadLook(preferences, "g", lookKind, -1, ledSettings.globalLook[look]);

    for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
      ledSettingsLoadLook(preferences, "z", lookKind, zone, ledSettings.zoneLook[look][zone]);
    }
  }

  preferences.end();
  ledSettingsLoadedSaved = true;
  return true;
}

bool ledSettingsSave() {
  Preferences preferences;

  if (!preferences.begin(LED_SETTINGS_NAMESPACE, false)) {
    return false;
  }

  bool ok = true;
  ok = preferences.putUShort(LED_SETTINGS_KEY_VERSION, LED_SETTINGS_VERSION) == sizeof(uint16_t) && ok;
  ok = preferences.putUChar(LED_SETTINGS_KEY_MASTER, ledSettings.masterBrightness) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(LED_SETTINGS_KEY_SATURATION, ledSettings.saturationScale) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(LED_SETTINGS_KEY_AMBIENT, ledSettings.ambientLevel) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(LED_SETTINGS_KEY_ACTIVE, ledSettings.activeLevel) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(LED_SETTINGS_KEY_SPEED, ledSettings.speedPercent) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(LED_SETTINGS_KEY_PALETTE, static_cast<uint8_t>(ledSettings.paletteMode)) == sizeof(uint8_t) && ok;
  ok = preferences.putUChar(LED_SETTINGS_KEY_BEHAVIOR, static_cast<uint8_t>(ledSettings.behaviorMode)) == sizeof(uint8_t) && ok;

  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    String key = ledSettingsZoneKey(i);
    ok = preferences.putUChar(key.c_str(), ledSettings.zoneBrightness[i]) == sizeof(uint8_t) && ok;
  }

  for (uint8_t look = 0; look < LED_LOOK_COUNT; look++) {
    LedLookKind lookKind = static_cast<LedLookKind>(look);
    ok = ledSettingsSaveLook(preferences, "g", lookKind, -1, ledSettings.globalLook[look]) && ok;

    for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
      ok = ledSettingsSaveLook(preferences, "z", lookKind, zone, ledSettings.zoneLook[look][zone]) && ok;
    }
  }

  preferences.end();
  ledSettingsLoadedSaved = ok;
  return ok;
}

void ledSettingsResetToDefaults() {
  ledSettings.masterBrightness = 255;
  ledSettings.saturationScale = 255;
  ledSettings.ambientLevel = 255;
  ledSettings.activeLevel = 255;
  ledSettings.speedPercent = 100;
  ledSettings.paletteMode = LED_PALETTE_DEFAULT;
  ledSettings.behaviorMode = LED_BEHAVIOR_NORMAL;

  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    ledSettings.zoneBrightness[i] = 255;
  }

  for (uint8_t look = 0; look < LED_LOOK_COUNT; look++) {
    ledSettingsResetLook(ledSettings.globalLook[look]);

    for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
      ledSettingsResetLook(ledSettings.zoneLook[look][zone]);
    }
  }
}

bool ledSettingsResetSavedToDefaults() {
  ledSettingsResetToDefaults();
  return ledSettingsSave();
}

const LedSettings& ledSettingsGet() {
  return ledSettings;
}

LedSettings& ledSettingsMutable() {
  return ledSettings;
}

uint8_t ledSettingsZoneBrightness(uint8_t zoneIndex) {
  if (zoneIndex >= LED_LOGICAL_ZONE_COUNT) {
    return 0;
  }

  return ledSettings.zoneBrightness[zoneIndex];
}

uint16_t ledSettingsVersion() {
  return LED_SETTINGS_VERSION;
}

bool ledSettingsLoadedFromSaved() {
  return ledSettingsLoadedSaved;
}

const LedLookSettings& ledSettingsGlobalLook(LedLookKind lookKind) {
  if (lookKind >= LED_LOOK_COUNT) {
    return ledSettings.globalLook[LED_LOOK_AMBIENT];
  }

  return ledSettings.globalLook[lookKind];
}

LedLookSettings& ledSettingsMutableGlobalLook(LedLookKind lookKind) {
  if (lookKind >= LED_LOOK_COUNT) {
    return ledSettings.globalLook[LED_LOOK_AMBIENT];
  }

  return ledSettings.globalLook[lookKind];
}

const LedLookSettings& ledSettingsZoneLook(LedLookKind lookKind, uint8_t zoneIndex) {
  if (lookKind >= LED_LOOK_COUNT || zoneIndex >= LED_LOGICAL_ZONE_COUNT) {
    return ledSettings.globalLook[LED_LOOK_AMBIENT];
  }

  return ledSettings.zoneLook[lookKind][zoneIndex];
}

LedLookSettings& ledSettingsMutableZoneLook(LedLookKind lookKind, uint8_t zoneIndex) {
  if (lookKind >= LED_LOOK_COUNT) {
    lookKind = LED_LOOK_AMBIENT;
  }

  if (zoneIndex >= LED_LOGICAL_ZONE_COUNT) {
    zoneIndex = 0;
  }

  return ledSettings.zoneLook[lookKind][zoneIndex];
}

const char *ledSettingsPaletteName(LedPaletteMode mode) {
  switch (mode) {
    case LED_PALETTE_WARM:
      return "warm";
    case LED_PALETTE_COOL:
      return "cool";
    case LED_PALETTE_EMBER:
      return "ember";
    case LED_PALETTE_OCEAN:
      return "ocean";
    case LED_PALETTE_RAINBOW:
      return "rainbow";
    case LED_PALETTE_DEFAULT:
    default:
      return "default";
  }
}

const char *ledSettingsLookKindName(LedLookKind lookKind) {
  switch (lookKind) {
    case LED_LOOK_ANIMATION:
      return "animation";
    case LED_LOOK_AMBIENT:
    default:
      return "ambient";
  }
}

const char *ledSettingsBehaviorName(LedBehaviorMode mode) {
  switch (mode) {
    case LED_BEHAVIOR_CALM:
      return "calm";
    case LED_BEHAVIOR_ENERGETIC:
      return "energetic";
    case LED_BEHAVIOR_SPARKLE:
      return "sparkle";
    case LED_BEHAVIOR_NORMAL:
    default:
      return "normal";
  }
}

bool ledSettingsParsePaletteName(const String &name, LedPaletteMode &mode) {
  String normalized = name;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "default") {
    mode = LED_PALETTE_DEFAULT;
  } else if (normalized == "warm") {
    mode = LED_PALETTE_WARM;
  } else if (normalized == "cool") {
    mode = LED_PALETTE_COOL;
  } else if (normalized == "ember") {
    mode = LED_PALETTE_EMBER;
  } else if (normalized == "ocean") {
    mode = LED_PALETTE_OCEAN;
  } else if (normalized == "rainbow") {
    mode = LED_PALETTE_RAINBOW;
  } else {
    return false;
  }

  return true;
}

bool ledSettingsParseBehaviorName(const String &name, LedBehaviorMode &mode) {
  String normalized = name;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "normal") {
    mode = LED_BEHAVIOR_NORMAL;
  } else if (normalized == "calm") {
    mode = LED_BEHAVIOR_CALM;
  } else if (normalized == "energetic") {
    mode = LED_BEHAVIOR_ENERGETIC;
  } else if (normalized == "sparkle") {
    mode = LED_BEHAVIOR_SPARKLE;
  } else {
    return false;
  }

  return true;
}

bool ledSettingsParseLookKindName(const String &name, LedLookKind &lookKind) {
  String normalized = name;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "ambient") {
    lookKind = LED_LOOK_AMBIENT;
  } else if (normalized == "animation") {
    lookKind = LED_LOOK_ANIMATION;
  } else {
    return false;
  }

  return true;
}

void ledSettingsPrint(Stream &out) {
  out.print("LED SETTINGS brightness=");
  out.print(ledSettings.masterBrightness);
  out.print(" saturation=");
  out.print(ledSettings.saturationScale);
  out.print(" ambient=");
  out.print(ledSettings.ambientLevel);
  out.print(" active=");
  out.print(ledSettings.activeLevel);
  out.print(" speed=");
  out.print(ledSettings.speedPercent);
  out.print(" palette=");
  out.print(ledSettingsPaletteName(ledSettings.paletteMode));
  out.print(" behavior=");
  out.print(ledSettingsBehaviorName(ledSettings.behaviorMode));
  out.print(" zones=");

  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    if (i > 0) {
      out.print(",");
    }

    out.print(ledSettings.zoneBrightness[i]);
  }

  out.print(" saved=");
  out.print(ledSettingsLoadedSaved ? "1" : "0");
  out.print(" version=");
  out.println(LED_SETTINGS_VERSION);

  for (uint8_t look = 0; look < LED_LOOK_COUNT; look++) {
    LedLookKind lookKind = static_cast<LedLookKind>(look);
    const LedLookSettings &globalLook = ledSettingsGlobalLook(lookKind);
    out.print("LED LOOK ");
    out.print(ledSettingsLookKindName(lookKind));
    out.print(" whole brightness=");
    out.print(globalLook.brightness);
    out.print(" saturation=");
    out.print(globalLook.saturation);
    out.print(" speed=");
    out.print(globalLook.speedPercent);
    out.print(" palette=");
    out.print(ledSettingsPaletteName(globalLook.paletteMode));
    out.print(" behavior=");
    out.println(ledSettingsBehaviorName(globalLook.behaviorMode));
  }
}
