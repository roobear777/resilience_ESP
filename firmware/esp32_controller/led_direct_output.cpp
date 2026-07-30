#include "led_direct_output.h"

#include <string.h>

#include "eclair_link_protocol.h"
#include "led_config.h"
#include "led_engine.h"
#include "led_settings.h"
#include "led_state.h"

constexpr uint8_t TARDI_ECLAIR_UART_RX_PIN = 41;
constexpr uint8_t TARDI_ECLAIR_UART_TX_PIN = 40;
constexpr uint32_t LED_STARTUP_HARDWARE_TEST_MS = 5000;
constexpr bool ENABLE_REAL_ECLAIR_OUTPUT = true;
constexpr LedOutputMode DEFAULT_LED_OUTPUT_MODE = LED_OUTPUT_ANIMATION;

static_assert(LED_OUTPUT_OFF == 0, "Eclair protocol mode mismatch");
static_assert(LED_OUTPUT_VALIDATE_SOLID == 1, "Eclair protocol mode mismatch");
static_assert(LED_OUTPUT_VALIDATE_CHANNEL == 2, "Eclair protocol mode mismatch");
static_assert(LED_OUTPUT_VALIDATE_COLOR == 3, "Eclair protocol mode mismatch");
static_assert(LED_OUTPUT_ANIMATION == 4, "Eclair protocol mode mismatch");
static_assert(LED_VALIDATION_COLOR_RED == 0 && LED_VALIDATION_COLOR_BLUE == 2, "Eclair protocol color mismatch");
static_assert(LED_LOGICAL_ZONE_COUNT == ECLAIR_WIRE_ZONE_COUNT, "Protocol zone count must match the LED engine");
static_assert(LED_LOOK_COUNT == ECLAIR_WIRE_LOOK_COUNT, "Protocol look count must match saved settings");

static HardwareSerial eclairLinkSerial(1);
static bool eclairLinkInitialized = false;
static LedOutputMode ledDirectRuntimeMode = DEFAULT_LED_OUTPUT_MODE;
static int8_t ledDirectValidationLane = -1;
static LedValidationColor ledDirectValidationColor = LED_VALIDATION_COLOR_RED;
static uint32_t eclairStateSequence = 0;
static uint32_t eclairLastStateSentMs = 0;
static uint32_t eclairLastStatusReceivedMs = 0;
static EclairStatusPacket eclairLastStatus = {};
static uint8_t eclairStatusBuffer[sizeof(EclairStatusPacket)] = { 0 };
static size_t eclairStatusBufferLength = 0;

static const char *ledDirectOutputValidationColorName() {
  switch (ledDirectValidationColor) {
    case LED_VALIDATION_COLOR_GREEN:
      return "green";
    case LED_VALIDATION_COLOR_BLUE:
      return "blue";
    case LED_VALIDATION_COLOR_RED:
    default:
      return "red";
  }
}

static void ledDirectCopyLookToWire(
  EclairWireLookSettings &wire,
  const LedLookSettings &look
) {
  wire.brightness = look.brightness;
  wire.saturation = look.saturation;
  wire.speedPercent = look.speedPercent;
  wire.paletteMode = static_cast<uint8_t>(look.paletteMode);
  wire.behaviorMode = static_cast<uint8_t>(look.behaviorMode);
}

static void ledDirectBuildStatePacket(EclairStatePacket &packet, uint32_t nowMs) {
  memset(&packet, 0, sizeof(packet));
  packet.magic = ECLAIR_STATE_MAGIC;
  packet.protocolVersion = ECLAIR_PROTOCOL_VERSION;
  packet.packetType = ECLAIR_PACKET_STATE;
  packet.packetSize = sizeof(packet);
  packet.sequence = ++eclairStateSequence;
  packet.senderNowMs = nowMs;
  packet.activeZoneMask = ledActiveZoneMask(nowMs);
  packet.outputMode = static_cast<uint8_t>(ledDirectRuntimeMode);
  packet.validationLane = ledDirectValidationLane > 0
    ? static_cast<uint8_t>(ledDirectValidationLane)
    : 0;
  packet.validationColor = static_cast<uint8_t>(ledDirectValidationColor);

  if (ledEngineIsPressureTestEnabled()) {
    packet.flags |= ECLAIR_FLAG_PRESSURE_TEST;
  }
  if (ledEngineIsAllGreenOverrideEnabled()) {
    packet.flags |= ECLAIR_FLAG_ALL_GREEN;
  }
  if (ledEngineIsStartupHardwareTestEnabled()) {
    packet.flags |= ECLAIR_FLAG_STARTUP_TEST;
  }

  const LedSettings &settings = ledSettingsGet();
  packet.masterBrightness = settings.masterBrightness;
  packet.saturationScale = settings.saturationScale;
  packet.ambientLevel = settings.ambientLevel;
  packet.activeLevel = settings.activeLevel;
  packet.speedPercent = settings.speedPercent;
  packet.animationDurationSeconds = settings.animationDurationSeconds;
  packet.paletteMode = static_cast<uint8_t>(settings.paletteMode);
  packet.behaviorMode = static_cast<uint8_t>(settings.behaviorMode);

  for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
    packet.zoneBrightness[zone] = settings.zoneBrightness[zone];
  }
  for (uint8_t look = 0; look < LED_LOOK_COUNT; look++) {
    ledDirectCopyLookToWire(packet.globalLook[look], settings.globalLook[look]);
    for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) {
      ledDirectCopyLookToWire(packet.zoneLook[look][zone], settings.zoneLook[look][zone]);
    }
  }

  eclairLinkFinalizePacket(packet);
}

static bool ledDirectStatusPacketIsValid(const EclairStatusPacket &packet) {
  return packet.magic == ECLAIR_STATUS_MAGIC
    && packet.protocolVersion == ECLAIR_PROTOCOL_VERSION
    && packet.packetType == ECLAIR_PACKET_STATUS
    && packet.packetSize == sizeof(packet)
    && eclairLinkPacketCrcIsValid(packet);
}

static void ledDirectReadStatus(uint32_t nowMs) {
  while (eclairLinkSerial.available() > 0) {
    eclairStatusBuffer[eclairStatusBufferLength++] = static_cast<uint8_t>(eclairLinkSerial.read());

    if (eclairStatusBufferLength < sizeof(EclairStatusPacket)) {
      continue;
    }

    EclairStatusPacket candidate;
    memcpy(&candidate, eclairStatusBuffer, sizeof(candidate));
    if (ledDirectStatusPacketIsValid(candidate)) {
      eclairLastStatus = candidate;
      eclairLastStatusReceivedMs = nowMs;
      eclairStatusBufferLength = 0;
      continue;
    }

    memmove(eclairStatusBuffer, eclairStatusBuffer + 1, sizeof(eclairStatusBuffer) - 1);
    eclairStatusBufferLength = sizeof(eclairStatusBuffer) - 1;
  }
}

static void ledDirectSendState(uint32_t nowMs, bool force) {
  if (!ENABLE_REAL_ECLAIR_OUTPUT || !eclairLinkInitialized) {
    return;
  }
  if (!force && (nowMs - eclairLastStateSentMs) < ECLAIR_STATE_INTERVAL_MS) {
    return;
  }

  EclairStatePacket packet;
  ledDirectBuildStatePacket(packet, nowMs);
  eclairLinkSerial.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  eclairLastStateSentMs = nowMs;
}

void ledDirectOutputBegin() {
  eclairLinkSerial.setRxBufferSize(512);
  eclairLinkSerial.begin(
    ECLAIR_LINK_BAUD,
    SERIAL_8N1,
    TARDI_ECLAIR_UART_RX_PIN,
    TARDI_ECLAIR_UART_TX_PIN
  );
  eclairLinkInitialized = true;
  ledDirectSendState(millis(), true);
  Serial.println("Eclair LED link: READY");
  Serial.println("Eclair UART1: Tardi TX GPIO40 -> Eclair RX GPIO18; Tardi RX GPIO41 <- Eclair TX GPIO17; 2000000 baud");
}

void ledDirectOutputUpdate(uint32_t nowMs) {
  ledDirectReadStatus(nowMs);
  ledDirectSendState(nowMs, false);
}

void ledDirectOutputRunStartupHardwareTest(Stream &out) {
  if (!ENABLE_REAL_ECLAIR_OUTPUT || !eclairLinkInitialized) {
    out.println("LED HARDWARE TEST ERROR: Eclair link is unavailable");
    return;
  }

  ledEngineSetStartupHardwareTestEnabled(true);
  out.println("LED HARDWARE TEST: requesting 5 seconds of moving Eclair animation, brightness 4-15%, speed 100%");

  uint32_t testStartMs = millis();
  ledDirectSendState(testStartMs, true);
  while ((millis() - testStartMs) < LED_STARTUP_HARDWARE_TEST_MS) {
    ledDirectOutputUpdate(millis());
    delay(1);
  }

  ledEngineSetStartupHardwareTestEnabled(false);
  ledDirectSendState(millis(), true);
  out.println("LED HARDWARE TEST COMPLETE: saved settings restored");
  if (!ledDirectOutputLinkOnline()) {
    out.println("WARNING: no Eclair status received during startup hardware test");
  }
  if (ledSettingsAmbientIsCompletelyDark()) {
    out.println("WARNING: saved ambient settings are completely dark");
  }
}

bool ledDirectOutputAllowed() {
  return ENABLE_REAL_ECLAIR_OUTPUT;
}

bool ledDirectOutputFirstShowAttempted() {
  return (eclairLastStatus.flags & ECLAIR_STATUS_FIRST_SHOW_ATTEMPTED) != 0;
}

bool ledDirectOutputLinkOnline() {
  return (eclairLastStatus.flags & ECLAIR_STATUS_LINK_VALID) != 0
    && eclairLastStatusReceivedMs != 0
    && (millis() - eclairLastStatusReceivedMs) <= ECLAIR_LINK_TIMEOUT_MS * 2;
}

const char *ledDirectOutputModeName() {
  switch (ledDirectRuntimeMode) {
    case LED_OUTPUT_VALIDATE_COLOR:
      return "VALIDATE_COLOR";
    case LED_OUTPUT_VALIDATE_SOLID:
      return "VALIDATE_SOLID";
    case LED_OUTPUT_VALIDATE_CHANNEL:
      return "VALIDATE_CHANNEL";
    case LED_OUTPUT_ANIMATION:
      return "ANIMATION";
    case LED_OUTPUT_OFF:
    default:
      return "OFF";
  }
}

bool ledDirectOutputSetMode(LedOutputMode mode, Stream &out) {
  if (mode == LED_OUTPUT_VALIDATE_CHANNEL) {
    out.println("Use: led ch 1..7");
    return false;
  }
  if (mode == LED_OUTPUT_VALIDATE_COLOR) {
    out.println("Use: led red, led green, or led blue");
    return false;
  }
  if (!ENABLE_REAL_ECLAIR_OUTPUT && mode != LED_OUTPUT_OFF) {
    out.println("LED real output blocked: ENABLE_REAL_ECLAIR_OUTPUT=false");
    return false;
  }

  ledDirectRuntimeMode = mode;
  ledDirectValidationLane = -1;
  ledDirectValidationColor = LED_VALIDATION_COLOR_RED;
  ledDirectSendState(millis(), true);
  out.print("LED mode: ");
  out.println(ledDirectOutputModeName());
  return true;
}

bool ledDirectOutputSetLaneValidationMode(uint8_t laneId, Stream &out) {
  if (laneId < 1 || laneId > LED_LOGICAL_ZONE_COUNT) {
    out.println("LED channel must be 1..7");
    return false;
  }
  if (!ENABLE_REAL_ECLAIR_OUTPUT) {
    out.println("LED real output blocked: ENABLE_REAL_ECLAIR_OUTPUT=false");
    return false;
  }

  ledDirectRuntimeMode = LED_OUTPUT_VALIDATE_CHANNEL;
  ledDirectValidationLane = laneId;
  ledDirectValidationColor = LED_VALIDATION_COLOR_RED;
  ledDirectSendState(millis(), true);
  out.print("LED mode: VALIDATE_CHANNEL ");
  out.println(laneId);
  return true;
}

bool ledDirectOutputSetColorValidationMode(LedValidationColor color, Stream &out) {
  if (!ENABLE_REAL_ECLAIR_OUTPUT) {
    out.println("LED real output blocked: ENABLE_REAL_ECLAIR_OUTPUT=false");
    return false;
  }

  ledDirectRuntimeMode = LED_OUTPUT_VALIDATE_COLOR;
  ledDirectValidationLane = -1;
  ledDirectValidationColor = color;
  ledDirectSendState(millis(), true);
  out.print("LED mode: VALIDATE_COLOR ");
  out.println(ledDirectOutputValidationColorName());
  return true;
}

void ledDirectOutputPrintRuntimeStatus(Stream &out) {
  out.print("LED backend=Eclair7/UART1/FastLED-RMT4 mode=");
  out.print(ledDirectOutputModeName());
  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_CHANNEL) {
    out.print(" ch=");
    out.print(ledDirectValidationLane);
  }
  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_COLOR) {
    out.print(" color=");
    out.print(ledDirectOutputValidationColorName());
  }
  out.print(" allowed=");
  out.print(ledDirectOutputAllowed() ? 1 : 0);
  out.print(" linkOnline=");
  out.print(ledDirectOutputLinkOnline() ? 1 : 0);
  out.print(" firstShowAttempted=");
  out.print(ledDirectOutputFirstShowAttempted() ? 1 : 0);
  out.print(" txSequence=");
  out.print(eclairStateSequence);
  out.print(" ackSequence=");
  out.print(eclairLastStatus.acknowledgedSequence);
  out.print(" frames=");
  out.print(eclairLastStatus.renderedFrames);
  out.print(" showUs=");
  out.print(eclairLastStatus.lastShowMicros);
  out.print(" remoteCrcErrors=");
  out.print(eclairLastStatus.crcErrorCount);
  out.print(" remoteTimeouts=");
  out.print(eclairLastStatus.linkTimeoutCount);
  out.print(" savedDark=");
  out.print(ledSettingsAmbientIsCompletelyDark() ? 1 : 0);
  out.print(" zones=7 pixels=");
  out.print(LED_TOTAL_PIXEL_COUNT);
  out.println(" tardi_uart_tx=40 tardi_uart_rx=41 eclair_led_pins=4,5,6,7,8,9,10 order=GRB");
}
