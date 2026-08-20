#ifndef TARDI_LED_LAYOUT_H
#define TARDI_LED_LAYOUT_H

#include <Arduino.h>
#include "led_config.h"

enum LedZoneIndex {
  LED_ZONE_Z1_MOUTH = 0,
  LED_ZONE_Z2_SHOULDER = 1,
  LED_ZONE_Z3_MIDBODY = 2,
  LED_ZONE_Z4_REAR = 3,
  LED_ZONE_Z5_FRONT_LEGS = 4,
  LED_ZONE_Z6_BACK_LEGS = 5,
  LED_ZONE_Z7_DIGESTIVE = 6,
  LED_ZONE_Z8_STATIONS = 7
};

const uint16_t LED_ZONE_START[LED_LOGICAL_ZONE_COUNT] = {
  0,    // Z1 mouth
  208,  // Z2 shoulder
  533,  // Z3 mid-body
  933,  // Z4 rear body
  1233, // Z5 front legs
  1533, // Z6 back legs
  1833, // Z7 digestive tract
  1908  // Z8 button-station strings
};

const uint16_t LED_ZONE_PIXEL_COUNT[LED_LOGICAL_ZONE_COUNT] = {
  208, // Z1 mouth: 0-207
  325, // Z2 shoulder: 208-532
  400, // Z3 mid-body: 533-932
  300, // Z4 rear body: 933-1232
  300, // Z5 front legs: 1233-1532
  300, // Z6 back legs: 1533-1832
  75,  // Z7 digestive tract: 1833-1907
  100  // Z8 button-station strings: 1908-2007
};

const uint16_t LED_ZONE_END[LED_LOGICAL_ZONE_COUNT] = {
  207,
  532,
  932,
  1232,
  1532,
  1832,
  1907,
  2007
};

inline bool ledLayoutZoneForPixel(uint16_t logicalPixelIndex, uint8_t &zoneIndex) {
  for (uint8_t i = 0; i < LED_LOGICAL_ZONE_COUNT; i++) {
    if (
      logicalPixelIndex >= LED_ZONE_START[i]
      && logicalPixelIndex <= LED_ZONE_END[i]
    ) {
      zoneIndex = i;
      return true;
    }
  }

  return false;
}

const uint8_t LED_OUTPUT_EXPANDER_CHANNEL_COUNT = 8;

// Physical Output Expander order differs from PixelBlaze logical render order.
const uint8_t LED_OUTPUT_EXPANDER_ZONE[LED_OUTPUT_EXPANDER_CHANNEL_COUNT] = {
  LED_ZONE_Z8_STATIONS,   // Ch0
  LED_ZONE_Z1_MOUTH,      // Ch1
  LED_ZONE_Z2_SHOULDER,   // Ch2
  LED_ZONE_Z3_MIDBODY,    // Ch3
  LED_ZONE_Z4_REAR,       // Ch4
  LED_ZONE_Z5_FRONT_LEGS, // Ch5
  LED_ZONE_Z6_BACK_LEGS,  // Ch6
  LED_ZONE_Z7_DIGESTIVE   // Ch7
};

const uint8_t LED_Z1_STRIP_COUNT = 4;
const uint16_t LED_Z1_STRIP_LENGTHS[LED_Z1_STRIP_COUNT] = {
  33,
  50,
  60,
  65
};

const uint8_t LED_Z2_STRIP_COUNT = 6;
const uint16_t LED_Z2_STRIP_LENGTHS[LED_Z2_STRIP_COUNT] = {
  44,
  48,
  52,
  56,
  60,
  65
};

const uint8_t LED_Z3_RING_COUNT = 8;
const uint16_t LED_Z3_PIXELS_PER_RING = 50;

const uint8_t LED_Z4_STRIP_COUNT = 12;
const uint16_t LED_Z4_PIXELS_PER_STRIP = 25;

const uint8_t LED_LEG_LOGICAL_COUNT = 8;
const uint16_t LED_LEG_PIXELS_PER_LEG = 75;
const uint8_t LED_LEG_OUTPUT_COUNT = 2;
const uint8_t LED_LEGS_PER_OUTPUT = 4;

const uint16_t LED_Z7_PIXEL_COUNT = 75;
const uint8_t LED_Z7_PARALLEL_STRAND_COUNT = 7;
const bool LED_Z7_USES_REVERSED_VISUAL_INDEX = true;

const uint8_t LED_Z8_STATION_STRING_COUNT = 7;
const uint16_t LED_Z8_PIXELS_PER_STATION_STRING = 14;
const uint16_t LED_Z8_SPARE_PIXEL_COUNT = 2;

#endif
