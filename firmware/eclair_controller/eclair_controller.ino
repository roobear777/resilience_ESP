#include <Arduino.h>

#include <FastLED.h>
#include <string.h>

#include "src/shared/eclair_link_protocol.h"
#include "src/shared/led_color_convert.h"
#include "src/shared/led_engine.h"
#include "src/shared/led_layout.h"
#include "src/shared/led_settings.h"
#include "src/shared/led_state.h"

#if !ARDUINO_USB_CDC_ON_BOOT
#error "Enable Tools > USB CDC On Boot. Eclair diagnostics must preserve native USB on GPIO19/GPIO20."
#endif

// Eclair: ESP32-S3-DevKitC-1 with ESP32-S3-WROOM-1-N8R8.
// Wiring is direct 3.3 V GPIO -> LED DIN, with common ESP/LED-power ground.
constexpr uint8_t ECLAIR_Z4_LED_PIN = 4;
constexpr uint8_t ECLAIR_Z5_LED_PIN = 5;
constexpr uint8_t ECLAIR_Z6_LED_PIN = 6;
constexpr uint8_t ECLAIR_Z7_LED_PIN = 7;
constexpr uint8_t ECLAIR_UART_RX_PIN = 18;
constexpr uint8_t ECLAIR_UART_TX_PIN = 17;
constexpr uint32_t ECLAIR_FRAME_INTERVAL_MS = 33;

enum EclairOutputMode : uint8_t {
  ECLAIR_OUTPUT_OFF = 0,
  ECLAIR_OUTPUT_VALIDATE_SOLID = 1,
  ECLAIR_OUTPUT_VALIDATE_CHANNEL = 2,
  ECLAIR_OUTPUT_VALIDATE_COLOR = 3,
  ECLAIR_OUTPUT_ANIMATION = 4
};

enum EclairValidationColor : uint8_t {
  ECLAIR_VALIDATION_RED = 0,
  ECLAIR_VALIDATION_GREEN = 1,
  ECLAIR_VALIDATION_BLUE = 2
};

static HardwareSerial tardiLinkSerial(1);
static CRGB eclairFrame[LED_TOTAL_PIXEL_COUNT];
static uint8_t eclairStateBuffer[sizeof(EclairStatePacket)] = { 0 };
static size_t eclairStateBufferLength = 0;
static bool eclairHasValidState = false;
static bool eclairOutputWasActive = false;
static bool eclairFirstShowAttempted = false;
static bool eclairLinkTimedOut = false;
static uint8_t eclairOutputMode = ECLAIR_OUTPUT_OFF;
static uint8_t eclairValidationLane = 0;
static uint8_t eclairValidationColor = ECLAIR_VALIDATION_RED;
static uint32_t eclairAcknowledgedSequence = 0;
static uint32_t eclairSenderNowAtReceive = 0;
static uint32_t eclairLastStateReceivedMs = 0;
static uint32_t eclairLastFrameMs = 0;
static uint32_t eclairLastStatusSentMs = 0;
static uint32_t eclairRenderedFrames = 0;
static uint32_t eclairLastShowMicros = 0;
static uint32_t eclairCrcErrorCount = 0;
static uint32_t eclairLinkTimeoutCount = 0;

static_assert(LED_LOGICAL_ZONE_COUNT == ECLAIR_WIRE_ZONE_COUNT, "Protocol zone count must match the LED engine");
static_assert(LED_LOOK_COUNT == ECLAIR_WIRE_LOOK_COUNT, "Protocol look count must match saved settings");
static_assert(ECLAIR_OUTPUT_OFF == 0 && ECLAIR_OUTPUT_ANIMATION == 4, "Output mode values must match Tardi");
static_assert(ECLAIR_VALIDATION_RED == 0 && ECLAIR_VALIDATION_BLUE == 2, "Validation colors must match Tardi");

static LedPaletteMode eclairSafePalette(uint8_t value) {
  return value <= LED_PALETTE_RAINBOW
    ? static_cast<LedPaletteMode>(value)
    : LED_PALETTE_DEFAULT;
}

static LedBehaviorMode eclairSafeBehavior(uint8_t value) {
  return value <= LED_BEHAVIOR_SPARKLE
    ? static_cast<LedBehaviorMode>(value)
    : LED_BEHAVIOR_NORMAL;
}

static void eclairApplyWireLook(
  LedLookSettings &look,
  const EclairWireLookSettings &wire
) {
  look.brightness = wire.brightness;
  look.saturation = wire.saturation;
  look.speedPercent = wire.speedPercent;
  look.paletteMode = eclairSafePalette(wire.paletteMode);
  look.behaviorMode = eclairSafeBehavior(wire.behaviorMode);
}

static bool eclairStatePacketIsValid(const EclairStatePacket &packet) {
  return packet.magic == ECLAIR_STATE_MAGIC
    && packet.protocolVersion == ECLAIR_PROTOCOL_VERSION
    && packet.packetType == ECLAIR_PACKET_STATE
    && packet.packetSize == sizeof(packet)
    && packet.outputMode <= ECLAIR_OUTPUT_ANIMATION
    && eclairLinkPacketCrcIsValid(packet);
}

static void eclairApplyState(const EclairStatePacket &packet, uint32_t nowMs) {
  LedSettings &settings = ledSettingsMutable();
  settings.masterBrightness = packet.masterBrightness;
  settings.saturationScale = packet.saturationScale;
  settings.ambientLevel = packet.ambientLevel;
  settings.activeLevel = packet.activeLevel;
  settings.speedPercent = packet.speedPercent;
  settings.animationDurationSeconds = packet.animationDurationSeconds;
  settings.paletteMode = eclairSafePalette(packet.paletteMode);
  settings.behaviorMode = eclairSafeBehavior(packet.behaviorMode);

  for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
    settings.zoneBrightness[zone] = packet.zoneBrightness[zone];
  }
  for (uint8_t look = 0; look < LED_LOOK_COUNT; look++) {
    eclairApplyWireLook(settings.globalLook[look], packet.globalLook[look]);
    for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
      eclairApplyWireLook(settings.zoneLook[look][zone], packet.zoneLook[look][zone]);
    }
  }

  ledUseRemoteActiveZoneMask(packet.activeZoneMask);
  ledEngineSetPressureTestEnabled((packet.flags & ECLAIR_FLAG_PRESSURE_TEST) != 0);
  ledEngineSetAllGreenOverride((packet.flags & ECLAIR_FLAG_ALL_GREEN) != 0);
  ledEngineSetStartupHardwareTestEnabled((packet.flags & ECLAIR_FLAG_STARTUP_TEST) != 0);

  eclairOutputMode = packet.outputMode;
  eclairValidationLane = packet.validationLane;
  eclairValidationColor = packet.validationColor;
  eclairAcknowledgedSequence = packet.sequence;
  eclairSenderNowAtReceive = packet.senderNowMs;
  eclairLastStateReceivedMs = nowMs;
  eclairHasValidState = true;
  eclairLinkTimedOut = false;
}

static void eclairReadState(uint32_t nowMs) {
  while (tardiLinkSerial.available() > 0) {
    eclairStateBuffer[eclairStateBufferLength++] = static_cast<uint8_t>(tardiLinkSerial.read());

    if (eclairStateBufferLength < sizeof(EclairStatePacket)) {
      continue;
    }

    EclairStatePacket candidate;
    memcpy(&candidate, eclairStateBuffer, sizeof(candidate));
    if (eclairStatePacketIsValid(candidate)) {
      eclairApplyState(candidate, nowMs);
      eclairStateBufferLength = 0;
      continue;
    }

    // Only a candidate with the complete expected header is a CRC failure.
    // Other rejected windows are normal byte-by-byte framing recovery.
    if (candidate.magic == ECLAIR_STATE_MAGIC
        && candidate.protocolVersion == ECLAIR_PROTOCOL_VERSION
        && candidate.packetType == ECLAIR_PACKET_STATE
        && candidate.packetSize == sizeof(candidate)
        && !eclairLinkPacketCrcIsValid(candidate)) {
      eclairCrcErrorCount++;
    }
    memmove(eclairStateBuffer, eclairStateBuffer + 1, sizeof(eclairStateBuffer) - 1);
    eclairStateBufferLength = sizeof(eclairStateBuffer) - 1;
  }
}

static LedRgbColor eclairValidationRgbForLane(uint8_t laneId) {
  static const LedRgbColor colors[8] = {
    { 0, 0, 0 }, { 8, 8, 8 }, { 0, 12, 12 }, { 0, 0, 14 },
    { 10, 0, 14 }, { 14, 6, 0 }, { 14, 10, 0 }, { 14, 0, 0 }
  };
  return laneId < 8 ? colors[laneId] : LedRgbColor{ 0, 0, 0 };
}

static LedRgbColor eclairValidationRgbForColor() {
  switch (eclairValidationColor) {
    case ECLAIR_VALIDATION_GREEN:
      return { 0, 12, 0 };
    case ECLAIR_VALIDATION_BLUE:
      return { 0, 0, 12 };
    case ECLAIR_VALIDATION_RED:
    default:
      return { 12, 0, 0 };
  }
}

static LedRgbColor eclairRenderRgb(uint16_t pixelIndex, uint32_t remoteNowMs) {
  switch (eclairOutputMode) {
    case ECLAIR_OUTPUT_VALIDATE_SOLID:
      return { 8, 8, 8 };
    case ECLAIR_OUTPUT_VALIDATE_CHANNEL: {
      uint8_t zoneIndex = 0;
      if (!ledLayoutZoneForPixel(pixelIndex, zoneIndex) || eclairValidationLane != zoneIndex + 1) {
        return { 0, 0, 0 };
      }
      return eclairValidationRgbForLane(eclairValidationLane);
    }
    case ECLAIR_OUTPUT_VALIDATE_COLOR:
      return eclairValidationRgbForColor();
    case ECLAIR_OUTPUT_ANIMATION:
      return ledColorToRgb(ledEngineRenderPixel(pixelIndex, remoteNowMs));
    case ECLAIR_OUTPUT_OFF:
    default:
      return { 0, 0, 0 };
  }
}

static void eclairShowBlack() {
  fill_solid(eclairFrame, LED_TOTAL_PIXEL_COUNT, CRGB::Black);
  eclairFirstShowAttempted = true;
  uint32_t startedUs = micros();
  FastLED.show();
  eclairLastShowMicros = micros() - startedUs;
  eclairOutputWasActive = false;
}

static void eclairRenderFrame(uint32_t nowMs) {
  if (!eclairHasValidState || eclairLinkTimedOut) {
    return;
  }
  if ((nowMs - eclairLastFrameMs) < ECLAIR_FRAME_INTERVAL_MS) {
    return;
  }
  eclairLastFrameMs = nowMs;

  uint32_t remoteNowMs = eclairSenderNowAtReceive + (nowMs - eclairLastStateReceivedMs);
  ledEngineUpdate(remoteNowMs);
  for (uint16_t pixel = LED_ZONE_START[LED_ZONE_Z4_REAR]; pixel < LED_TOTAL_PIXEL_COUNT; pixel++) {
    LedRgbColor rgb = eclairRenderRgb(pixel, remoteNowMs);
    eclairFrame[pixel] = CRGB(rgb.r, rgb.g, rgb.b);
  }

  eclairFirstShowAttempted = true;
  uint32_t startedUs = micros();
  FastLED.show();
  eclairLastShowMicros = micros() - startedUs;
  eclairRenderedFrames++;
  eclairOutputWasActive = eclairOutputMode != ECLAIR_OUTPUT_OFF;
}

static void eclairCheckLinkTimeout(uint32_t nowMs) {
  if (!eclairHasValidState || (nowMs - eclairLastStateReceivedMs) <= ECLAIR_LINK_TIMEOUT_MS) {
    return;
  }
  if (!eclairLinkTimedOut) {
    eclairLinkTimedOut = true;
    eclairLinkTimeoutCount++;
    ledUseRemoteActiveZoneMask(0);
    ledEngineSetPressureTestEnabled(false);
    ledEngineSetAllGreenOverride(false);
    ledEngineSetStartupHardwareTestEnabled(false);
    eclairShowBlack();
    Serial.println("Eclair link timeout: LEDs forced black");
  }
}

static void eclairSendStatus(uint32_t nowMs) {
  if ((nowMs - eclairLastStatusSentMs) < ECLAIR_STATUS_INTERVAL_MS) {
    return;
  }
  eclairLastStatusSentMs = nowMs;

  EclairStatusPacket packet = {};
  packet.magic = ECLAIR_STATUS_MAGIC;
  packet.protocolVersion = ECLAIR_PROTOCOL_VERSION;
  packet.packetType = ECLAIR_PACKET_STATUS;
  packet.packetSize = sizeof(packet);
  packet.acknowledgedSequence = eclairAcknowledgedSequence;
  packet.receiverNowMs = nowMs;
  packet.renderedFrames = eclairRenderedFrames;
  packet.lastShowMicros = eclairLastShowMicros;
  packet.crcErrorCount = eclairCrcErrorCount;
  packet.linkTimeoutCount = eclairLinkTimeoutCount;
  if (eclairHasValidState && !eclairLinkTimedOut) {
    packet.flags |= ECLAIR_STATUS_LINK_VALID;
  }
  if (eclairFirstShowAttempted) {
    packet.flags |= ECLAIR_STATUS_FIRST_SHOW_ATTEMPTED;
  }
  if (eclairOutputWasActive) {
    packet.flags |= ECLAIR_STATUS_OUTPUT_ACTIVE;
  }
  eclairLinkFinalizePacket(packet);
  tardiLinkSerial.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

static void eclairRegisterLedControllers() {
  FastLED.addLeds<WS2812B, ECLAIR_Z4_LED_PIN, GRB>(eclairFrame + LED_ZONE_START[3], LED_ZONE_PIXEL_COUNT[3]);
  FastLED.addLeds<WS2812B, ECLAIR_Z5_LED_PIN, GRB>(eclairFrame + LED_ZONE_START[4], LED_ZONE_PIXEL_COUNT[4]);
  FastLED.addLeds<WS2812B, ECLAIR_Z6_LED_PIN, GRB>(eclairFrame + LED_ZONE_START[5], LED_ZONE_PIXEL_COUNT[5]);
  FastLED.addLeds<WS2812B, ECLAIR_Z7_LED_PIN, GRB>(eclairFrame + LED_ZONE_START[6], LED_ZONE_PIXEL_COUNT[6]);
  FastLED.setBrightness(255);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Eclair LED Twin Z4-Z7 controller starting...");

  ledStateBegin();
  ledSettingsBegin();
  ledEngineBegin();
  eclairRegisterLedControllers();
  eclairShowBlack();

  tardiLinkSerial.setRxBufferSize(1024);
  tardiLinkSerial.begin(ECLAIR_LINK_BAUD, SERIAL_8N1, ECLAIR_UART_RX_PIN, ECLAIR_UART_TX_PIN);
  Serial.println("Eclair UART1 RX=GPIO18 TX=GPIO17 baud=2000000");
  Serial.println("Eclair LED lanes Z4..Z7 GPIO4,5,6,7; pixels 300,300,300,75; GRB");
}

void loop() {
  uint32_t nowMs = millis();
  eclairReadState(nowMs);
  eclairCheckLinkTimeout(nowMs);
  eclairRenderFrame(nowMs);
  eclairSendStatus(nowMs);
  delay(1);
}
