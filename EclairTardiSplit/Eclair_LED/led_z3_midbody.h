#ifndef TARDI_LED_Z3_MIDBODY_H
#define TARDI_LED_Z3_MIDBODY_H

#include <Arduino.h>

#include "led_color.h"

void ledZ3MidbodyBegin();
void ledZ3MidbodyUpdate(uint32_t deltaMs);
LedColor ledZ3MidbodyRender(uint16_t localIndex, bool active, uint32_t nowMs);

#endif
