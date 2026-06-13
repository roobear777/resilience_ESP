#ifndef TARDI_LED_LEGS_H
#define TARDI_LED_LEGS_H

#include <Arduino.h>

#include "led_color.h"

void ledLegsBegin();
void ledLegsUpdate(uint32_t deltaMs);
LedColor ledLegsRender(uint16_t localIndex, uint8_t outputZoneIndex, bool active, uint32_t nowMs);

#endif
