#include "led_state.h"

#include "led_config.h"
#include "led_settings.h"

static uint32_t ledActiveUntil[LED_LOGICAL_ZONE_COUNT] = { 0 };
static bool ledRemoteActiveMaskEnabled = false;
static uint8_t ledRemoteActiveMask = 0;

void ledStateBegin() {
  ledRemoteActiveMaskEnabled = false;
  ledRemoteActiveMask = 0;
  ledClearAllZones();
}

void ledTriggerZone(uint8_t zoneIndex, uint32_t nowMs) {
  if (zoneIndex >= LED_LOGICAL_ZONE_COUNT) {
    return;
  }

  ledActiveUntil[zoneIndex] = nowMs + ledSettingsAnimationDurationMs();
}

void ledActivateAllZones(uint32_t nowMs) {
  uint32_t activeUntil = nowMs + ledSettingsAnimationDurationMs();

  for (uint8_t zoneIndex = 0; zoneIndex < LED_LOGICAL_ZONE_COUNT; zoneIndex++) {
    ledActiveUntil[zoneIndex] = activeUntil;
  }
}

bool ledIsZoneActive(uint8_t zoneIndex, uint32_t nowMs) {
  if (zoneIndex >= LED_LOGICAL_ZONE_COUNT) {
    return false;
  }

  if (ledRemoteActiveMaskEnabled) {
    return (ledRemoteActiveMask & (1u << zoneIndex)) != 0;
  }

  return nowMs < ledActiveUntil[zoneIndex];
}

uint8_t ledActiveZoneMask(uint32_t nowMs) {
  uint8_t mask = 0;
  for (uint8_t zoneIndex = 0; zoneIndex < LED_LOGICAL_ZONE_COUNT; zoneIndex++) {
    if (ledIsZoneActive(zoneIndex, nowMs)) {
      mask |= static_cast<uint8_t>(1u << zoneIndex);
    }
  }
  return mask;
}

void ledUseRemoteActiveZoneMask(uint8_t activeZoneMask) {
  ledRemoteActiveMask = activeZoneMask & 0x7F;
  ledRemoteActiveMaskEnabled = true;
}

void ledStopUsingRemoteActiveZoneMask() {
  ledRemoteActiveMask = 0;
  ledRemoteActiveMaskEnabled = false;
}

void ledClearAllZones() {
  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    ledActiveUntil[i] = 0;
  }
}
