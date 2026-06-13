#ifndef TARDI_LED_Z7_DIGESTIVE_H
#define TARDI_LED_Z7_DIGESTIVE_H

#include <Arduino.h>

#include "led_color.h"

void ledZ7DigestiveBegin();
void ledZ7DigestiveUpdate(uint32_t deltaMs);
LedColor ledZ7DigestiveRender(uint16_t localIndex, bool active, uint32_t nowMs);

#endif
