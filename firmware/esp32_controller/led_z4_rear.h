#ifndef TARDI_LED_Z4_REAR_H
#define TARDI_LED_Z4_REAR_H

#include <Arduino.h>

#include "led_color.h"

void ledZ4RearBegin();
void ledZ4RearUpdate(uint32_t deltaMs);
LedColor ledZ4RearRender(uint16_t localIndex, bool active, uint32_t nowMs);

#endif
