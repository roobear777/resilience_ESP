#ifndef TARDI_LED_CONFIG_H
#define TARDI_LED_CONFIG_H

#include <Arduino.h>

// Logical LED zones follow the PixelBlaze layout order: Z1 through Z8.
const uint8_t LED_LOGICAL_ZONE_COUNT = 8;

// Default active animation window after an accepted ESP32 controller trigger.
const uint32_t LED_ACTIVE_WINDOW_MS = 10000;

const uint16_t LED_TOTAL_PIXEL_COUNT = 2008;

#endif
