#ifndef TARDI_LED_ENGINE_H
#define TARDI_LED_ENGINE_H

#include <Arduino.h>

#include "led_color.h"

void ledEngineBegin();
void ledEngineUpdate(uint32_t nowMs);
bool ledEngineIsPressureTestEnabled();
bool ledEngineIsAllGreenOverrideEnabled();
bool ledEngineIsStartupHardwareTestEnabled();
void ledEngineSetPressureTestEnabled(bool enabled);
void ledEngineSetAllGreenOverride(bool enabled);
void ledEngineSetStartupHardwareTestEnabled(bool enabled);
bool ledEngineIsZoneActive(uint8_t zoneIndex, uint32_t nowMs);
LedColor ledEngineRenderPixel(uint16_t logicalPixelIndex, uint32_t nowMs);

#endif
