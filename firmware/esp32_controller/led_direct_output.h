#ifndef TARDI_LED_DIRECT_OUTPUT_H
#define TARDI_LED_DIRECT_OUTPUT_H

#include <Arduino.h>

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
void ledDirectOutputRunStartupHardwareTest(Stream &out);
void ledDirectOutputUpdate(uint32_t nowMs);
bool ledDirectOutputAllowed();
bool ledDirectOutputFirstShowAttempted();
const char *ledDirectOutputModeName();
bool ledDirectOutputSetMode(LedOutputMode mode, Stream &out);
bool ledDirectOutputSetLaneValidationMode(uint8_t laneId, Stream &out);
bool ledDirectOutputSetColorValidationMode(LedValidationColor color, Stream &out);
void ledDirectOutputPrintRuntimeStatus(Stream &out);

#endif
