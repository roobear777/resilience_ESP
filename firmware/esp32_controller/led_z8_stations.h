#ifndef TARDI_LED_Z8_STATIONS_H
#define TARDI_LED_Z8_STATIONS_H

#include <Arduino.h>

#include "led_color.h"

void ledZ8StationsBegin();
void ledZ8StationsUpdate(uint32_t deltaMs);
uint8_t ledZ8StationFor(uint16_t localIndex);
LedColor ledZ8StationsRender(uint16_t localIndex, bool stationActive, uint32_t nowMs);

#endif
