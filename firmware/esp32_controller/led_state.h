#ifndef TARDI_LED_STATE_H
#define TARDI_LED_STATE_H

#include <Arduino.h>

void ledStateBegin();
void ledTriggerZone(uint8_t zoneIndex, uint32_t nowMs);
bool ledIsZoneActive(uint8_t zoneIndex, uint32_t nowMs);
void ledClearAllZones();

#endif
