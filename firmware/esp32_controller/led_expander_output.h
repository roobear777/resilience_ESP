#ifndef TARDI_LED_EXPANDER_OUTPUT_H
#define TARDI_LED_EXPANDER_OUTPUT_H

#include <Arduino.h>

#include "led_color_convert.h"

struct LedExpanderChannelFrameStats {
  uint8_t channelId;
  uint16_t logicalStartIndex;
  uint16_t pixelCount;
  uint32_t checksum;
  uint8_t firstPixel[3];
  uint8_t lastPixel[3];
};

struct LedExpanderFrameStats {
  uint8_t channelCount;
  uint16_t totalPixels;
  uint16_t failedPixels;
  uint32_t frameChecksum;
  LedExpanderChannelFrameStats channels[8];
};

enum LedOutputMode {
  LED_OUTPUT_OFF,
  LED_OUTPUT_VALIDATE_SOLID,
  LED_OUTPUT_VALIDATE_CHANNEL,
  LED_OUTPUT_ANIMATION
};

void ledExpanderOutputBegin();
void ledExpanderOutputUpdate(uint32_t nowMs);
bool ledExpanderOutputIsEnabled();
void ledExpanderOutputSetEnabled(bool enabled);
bool ledExpanderOutputIsInitialized();
bool ledExpanderOutputRealOutputAllowed();
bool ledExpanderOutputRealOutputStarted();
uint8_t ledExpanderOutputConfiguredChannelCount();
uint16_t ledExpanderOutputConfiguredPixelCount();
uint32_t ledExpanderOutputPlannedBaudRate();
int ledExpanderOutputPlannedTxPin();
LedOutputMode ledExpanderOutputMode();
int ledExpanderOutputValidationChannel();
const char *ledExpanderOutputModeName();
bool ledExpanderOutputSetMode(LedOutputMode mode, Stream &out);
bool ledExpanderOutputSetChannelValidationMode(uint8_t channelId, Stream &out);
void ledExpanderOutputPrintRuntimeStatus(Stream &out);
uint16_t ledExpanderOutputChannelPixelCount(uint8_t channelId);
uint16_t ledExpanderOutputChannelLogicalStart(uint8_t channelId);
bool ledExpanderOutputRenderPixelForTest(
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  LedRgbColor &outColor
);
bool ledExpanderOutputRenderChannelForTest(
  uint8_t channelId,
  uint32_t nowMs,
  LedRgbColor *outPixels,
  uint16_t outCapacity,
  uint16_t &outPixelCount
);
bool ledExpanderOutputRenderPackedPixelForTest(
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  uint8_t *outBytes,
  uint8_t outCapacity
);
bool ledExpanderOutputPrepareCallbacksForTest(uint32_t nowMs);
bool ledExpanderOutputSimulateFrameForTest(
  uint32_t nowMs,
  LedExpanderFrameStats &outStats
);
void ledExpanderOutputPrintSimFrameStats(uint32_t nowMs, Stream &out);

#endif
