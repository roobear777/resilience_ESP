#include "led_state.h"

#include "led_config.h"
#include "led_settings.h"

static uint32_t ledActiveUntil[LED_LOGICAL_ZONE_COUNT] = { 0 };

void ledStateBegin() {
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

  return nowMs < ledActiveUntil[zoneIndex];
}

void ledClearAllZones() {
  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    ledActiveUntil[i] = 0;
  }
}
