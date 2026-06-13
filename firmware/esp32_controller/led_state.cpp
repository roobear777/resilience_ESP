#include "led_state.h"

#include "led_config.h"

static uint32_t ledActiveUntil[LED_LOGICAL_ZONE_COUNT] = { 0 };

void ledStateBegin() {
  ledClearAllZones();
}

void ledTriggerZone(uint8_t zoneIndex, uint32_t nowMs) {
  if (zoneIndex >= LED_LOGICAL_ZONE_COUNT) {
    return;
  }

  ledActiveUntil[zoneIndex] = nowMs + LED_ACTIVE_WINDOW_MS;
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
