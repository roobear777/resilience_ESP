#ifndef ECLAIR_LED_DIRECT_OUTPUT_H
#define ECLAIR_LED_DIRECT_OUTPUT_H

#include <Arduino.h>

#include "led_color_convert.h"

// =============================================================================
// DIRECT LED OUTPUT — replaces the Pixelblaze Output Expander
// =============================================================================
//
// The Output Expander did three jobs: routing, 5V level shifting, and 100 ohm
// series termination. Routing now happens here in firmware (each lane is a
// slice of one framebuffer). Level shifting and termination move to an
// SN74AHCT244 buffer board — see docs/buffer_board.md.
//
// Output is FastLED's LCD_CLOCKLESS driver, which uses the ESP32-S3 LCD_CAM /
// I80 peripheral to clock all lanes out SIMULTANEOUSLY via DMA.
//
// Why not RMT: the ESP32-S3 has only FOUR RMT TX channels, and RMT on the S3
// has no DMA (it is CPU-serviced). Eight lanes is impossible there. Verified
// on hardware: 7/7 lanes reported LCD_CLOCKLESS, ~22 ms/frame at 43 fps.
//
// The API deliberately mirrors the old led_expander_output.h so the rest of
// the firmware and the web UI did not have to change.
// =============================================================================

struct LedLaneFrameStats {
  uint8_t laneId;
  uint16_t logicalStartIndex;
  uint16_t pixelCount;
  char driverName[24];
};

struct LedDirectFrameStats {
  uint8_t laneCount;
  uint16_t totalPixels;
  uint8_t lanesOnLcd;
  uint32_t lastShowMicros;
  LedLaneFrameStats lanes[8];
};

enum LedOutputMode {
  LED_OUTPUT_OFF,
  LED_OUTPUT_VALIDATE_SOLID,
  LED_OUTPUT_VALIDATE_CHANNEL,
  LED_OUTPUT_VALIDATE_COLOR,
  LED_OUTPUT_ANIMATION
};

enum LedValidationColor {
  LED_VALIDATION_COLOR_RED,
  LED_VALIDATION_COLOR_GREEN,
  LED_VALIDATION_COLOR_BLUE
};

void ledDirectOutputBegin();
void ledDirectOutputUpdate(uint32_t nowMs);

bool ledDirectOutputIsInitialized();
bool ledDirectOutputRealOutputAllowed();
bool ledDirectOutputRealOutputStarted();
bool ledDirectOutputAllLanesOnLcd();

uint8_t ledDirectOutputConfiguredLaneCount();
uint16_t ledDirectOutputConfiguredPixelCount();
uint8_t ledDirectOutputLanesOnLcd();

int ledDirectOutputLanePin(uint8_t laneId);
uint16_t ledDirectOutputLanePixelCount(uint8_t laneId);
uint16_t ledDirectOutputLaneLogicalStart(uint8_t laneId);
const char *ledDirectOutputLaneDriverName(uint8_t laneId);

uint32_t ledDirectOutputLastShowMicros();
float ledDirectOutputAverageShowMs();
float ledDirectOutputFps();
uint32_t ledDirectOutputExpectedTransmitMicros();

LedOutputMode ledDirectOutputMode();
int ledDirectOutputValidationChannel();
LedValidationColor ledDirectOutputValidationColor();
const char *ledDirectOutputModeName();
const char *ledDirectOutputValidationColorName();

bool ledDirectOutputSetMode(LedOutputMode mode, Stream &out);
bool ledDirectOutputSetChannelValidationMode(uint8_t laneId, Stream &out);
bool ledDirectOutputSetColorValidationMode(LedValidationColor color, Stream &out);

void ledDirectOutputPrintRuntimeStatus(Stream &out);
void ledDirectOutputPrintDriverTable(Stream &out);
void ledDirectOutputPrintHeap(const char *phase, Stream &out);

bool ledDirectOutputRenderPixelForTest(
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  LedRgbColor &outColor
);

#endif
