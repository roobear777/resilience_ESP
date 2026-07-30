#ifndef TARDI_LED_Z1_MOUTH_H
#define TARDI_LED_Z1_MOUTH_H

#include <Arduino.h>

#include "led_color.h"

void ledZ1MouthBegin();
void ledZ1MouthUpdate(uint32_t deltaMs);
LedColor ledZ1MouthRender(uint16_t localIndex, bool active, uint32_t nowMs);

#endif
