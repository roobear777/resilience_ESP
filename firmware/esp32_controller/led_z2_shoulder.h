#ifndef TARDI_LED_Z2_SHOULDER_H
#define TARDI_LED_Z2_SHOULDER_H

#include <Arduino.h>

#include "led_color.h"

void ledZ2ShoulderBegin();
void ledZ2ShoulderUpdate(uint32_t deltaMs);
LedColor ledZ2ShoulderRender(uint16_t localIndex, bool active, uint32_t nowMs);

#endif
