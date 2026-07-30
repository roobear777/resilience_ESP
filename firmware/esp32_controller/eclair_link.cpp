#include "eclair_link.h"
#include <string.h>
#include "eclair_link_protocol.h"
#include "led_direct_output.h"
#include "led_engine.h"
#include "led_settings.h"
#include "led_state.h"

constexpr uint8_t TARDI_ECLAIR_UART_RX_PIN = 41;
constexpr uint8_t TARDI_ECLAIR_UART_TX_PIN = 40;
static HardwareSerial linkSerial(1);
static bool initialized = false;
static uint32_t sequence = 0;
static uint32_t lastSentMs = 0;
static uint32_t lastStatusMs = 0;
static EclairStatusPacket lastStatus = {};
static uint8_t buffer[sizeof(EclairStatusPacket)] = {};
static size_t bufferLength = 0;

static_assert(LED_LOGICAL_ZONE_COUNT == ECLAIR_WIRE_ZONE_COUNT, "Protocol zone count must match the LED engine");
static_assert(LED_LOOK_COUNT == ECLAIR_WIRE_LOOK_COUNT, "Protocol look count must match saved settings");
static_assert(LED_OUTPUT_OFF == 0 && LED_OUTPUT_ANIMATION == 4, "LED output mode values are part of the wire protocol");
static_assert(LED_VALIDATION_COLOR_RED == 0 && LED_VALIDATION_COLOR_BLUE == 2, "Validation color values are part of the wire protocol");

static void copyLook(EclairWireLookSettings &wire, const LedLookSettings &look) {
  wire.brightness = look.brightness;
  wire.saturation = look.saturation;
  wire.speedPercent = look.speedPercent;
  wire.paletteMode = static_cast<uint8_t>(look.paletteMode);
  wire.behaviorMode = static_cast<uint8_t>(look.behaviorMode);
}

static void buildPacket(EclairStatePacket &packet, uint32_t nowMs) {
  memset(&packet, 0, sizeof(packet));
  packet.magic = ECLAIR_STATE_MAGIC;
  packet.protocolVersion = ECLAIR_PROTOCOL_VERSION;
  packet.packetType = ECLAIR_PACKET_STATE;
  packet.packetSize = sizeof(packet);
  packet.sequence = ++sequence;
  packet.senderNowMs = nowMs;
  packet.activeZoneMask = ledActiveZoneMask(nowMs);
  packet.outputMode = static_cast<uint8_t>(ledDirectOutputMode());
  packet.validationLane = ledDirectOutputValidationLane();
  packet.validationColor = static_cast<uint8_t>(ledDirectOutputValidationColor());
  if (ledEngineIsPressureTestEnabled()) packet.flags |= ECLAIR_FLAG_PRESSURE_TEST;
  if (ledEngineIsAllGreenOverrideEnabled()) packet.flags |= ECLAIR_FLAG_ALL_GREEN;
  if (ledEngineIsStartupHardwareTestEnabled()) packet.flags |= ECLAIR_FLAG_STARTUP_TEST;

  const LedSettings &settings = ledSettingsGet();
  packet.masterBrightness = settings.masterBrightness;
  packet.saturationScale = settings.saturationScale;
  packet.ambientLevel = settings.ambientLevel;
  packet.activeLevel = settings.activeLevel;
  packet.speedPercent = settings.speedPercent;
  packet.animationDurationSeconds = settings.animationDurationSeconds;
  packet.paletteMode = static_cast<uint8_t>(settings.paletteMode);
  packet.behaviorMode = static_cast<uint8_t>(settings.behaviorMode);
  for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) packet.zoneBrightness[zone] = settings.zoneBrightness[zone];
  for (uint8_t look = 0; look < LED_LOOK_COUNT; look++) {
    copyLook(packet.globalLook[look], settings.globalLook[look]);
    for (uint8_t zone = 0; zone < LED_LOGICAL_ZONE_COUNT; zone++) copyLook(packet.zoneLook[look][zone], settings.zoneLook[look][zone]);
  }
  eclairLinkFinalizePacket(packet);
}

static bool valid(const EclairStatusPacket &packet) {
  return packet.magic == ECLAIR_STATUS_MAGIC
    && packet.protocolVersion == ECLAIR_PROTOCOL_VERSION
    && packet.packetType == ECLAIR_PACKET_STATUS
    && packet.packetSize == sizeof(packet)
    && eclairLinkPacketCrcIsValid(packet);
}

static void readStatus(uint32_t nowMs) {
  while (linkSerial.available() > 0) {
    buffer[bufferLength++] = static_cast<uint8_t>(linkSerial.read());
    if (bufferLength < sizeof(EclairStatusPacket)) continue;
    EclairStatusPacket candidate;
    memcpy(&candidate, buffer, sizeof(candidate));
    if (valid(candidate)) {
      lastStatus = candidate;
      lastStatusMs = nowMs;
      bufferLength = 0;
    } else {
      memmove(buffer, buffer + 1, sizeof(buffer) - 1);
      bufferLength = sizeof(buffer) - 1;
    }
  }
}

void eclairLinkBegin() {
  linkSerial.setRxBufferSize(512);
  linkSerial.begin(ECLAIR_LINK_BAUD, SERIAL_8N1, TARDI_ECLAIR_UART_RX_PIN, TARDI_ECLAIR_UART_TX_PIN);
  initialized = true;
  lastSentMs = millis() - ECLAIR_STATE_INTERVAL_MS;
  Serial.println("Eclair link READY: UART1 TX=GPIO40 RX=GPIO41 baud=2000000");
}

void eclairLinkUpdate(uint32_t nowMs) {
  if (!initialized) return;
  readStatus(nowMs);
  if ((nowMs - lastSentMs) < ECLAIR_STATE_INTERVAL_MS) return;
  EclairStatePacket packet;
  buildPacket(packet, nowMs);
  linkSerial.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  lastSentMs = nowMs;
}

bool eclairLinkOnline() {
  return lastStatusMs != 0
    && (millis() - lastStatusMs) <= ECLAIR_LINK_TIMEOUT_MS * 2
    && (lastStatus.flags & ECLAIR_STATUS_LINK_VALID) != 0;
}

bool eclairLinkFirstShowAttempted() {
  return (lastStatus.flags & ECLAIR_STATUS_FIRST_SHOW_ATTEMPTED) != 0;
}

void eclairLinkPrintStatus(Stream &out) {
  out.print(" Eclair linkOnline=");
  out.print(eclairLinkOnline() ? 1 : 0);
  out.print(" tx=");
  out.print(sequence);
  out.print(" ack=");
  out.print(lastStatus.acknowledgedSequence);
  out.print(" frames=");
  out.print(lastStatus.renderedFrames);
  out.print(" showUs=");
  out.print(lastStatus.lastShowMicros);
  out.print(" crcErrors=");
  out.print(lastStatus.crcErrorCount);
  out.print(" timeouts=");
  out.print(lastStatus.linkTimeoutCount);
}
