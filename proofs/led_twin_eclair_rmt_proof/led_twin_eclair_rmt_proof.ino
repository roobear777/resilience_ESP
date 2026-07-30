// LED Twin Éclair ordinary FastLED/RMT proof
// First-flash baseline:
//   ESP32 Arduino core 3.3.10
//   FastLED 3.10.4
//   ESP32-S3-DevKitC-1 / WROOM-1-N8R8
//
// Éclair drives Z4-Z7 and receives frame/mode commands from Tardi over UART.
// If Tardi is absent, it continues with a local test pattern so LED output can
// be proven independently.

#define FASTLED_RMT5 0
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL 48
#define FASTLED_RMT_MEM_BLOCKS 1
#define FASTLED_RMT_MAX_CHANNELS 4
#define FASTLED_RMT_SERIAL_DEBUG 1
#define FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS 1000
#define FASTLED_ESP32_ENABLE_LOGGING 1

#include <Arduino.h>
#include <FastLED.h>
#include <esp_arduino_version.h>
#include <esp_idf_version.h>
#include <esp_system.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This proof sketch is for ESP32-S3 only."
#endif

namespace {

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t LINK_BAUD = 2000000;
constexpr int LINK_RX_PIN = 18;
constexpr int LINK_TX_PIN = 17;
constexpr uint8_t TEST_BRIGHTNESS = 32;
constexpr uint32_t FRAME_INTERVAL_US = 33333;
constexpr uint32_t STALL_THRESHOLD_US = 50000;
constexpr uint32_t REPORT_INTERVAL_MS = 2000;
constexpr uint32_t LINK_FRESH_MS = 500;

constexpr uint16_t Z4_COUNT = 300;
constexpr uint16_t Z5_COUNT = 300;
constexpr uint16_t Z6_COUNT = 300;
constexpr uint16_t Z7_COUNT = 75;
constexpr uint16_t LOCAL_TOTAL_COUNT = Z4_COUNT + Z5_COUNT + Z6_COUNT + Z7_COUNT;
static_assert(LOCAL_TOTAL_COUNT == 975, "Éclair LED Twin total must be 975");

CRGB z4[Z4_COUNT];
CRGB z5[Z5_COUNT];
CRGB z6[Z6_COUNT];
CRGB z7[Z7_COUNT];

HardwareSerial linkSerial(1);

struct ShowStats {
  uint64_t totalUs = 0;
  uint32_t samples = 0;
  uint32_t minimumUs = UINT32_MAX;
  uint32_t maximumUs = 0;
  uint32_t stallCount = 0;

  void add(uint32_t elapsedUs) {
    totalUs += elapsedUs;
    samples++;
    if (elapsedUs < minimumUs) minimumUs = elapsedUs;
    if (elapsedUs > maximumUs) maximumUs = elapsedUs;
    if (elapsedUs > STALL_THRESHOLD_US) stallCount++;
  }

  uint32_t averageUs() const {
    return samples == 0 ? 0 : static_cast<uint32_t>(totalUs / samples);
  }

  uint32_t printableMinimumUs() const {
    return samples == 0 ? 0 : minimumUs;
  }
};

ShowStats showStats;
uint32_t localFrameNumber = 0;
uint32_t commandedFrameNumber = 0;
uint8_t commandedMode = 0;
uint32_t lastCommandMs = 0;
uint32_t nextFrameUs = 0;
uint32_t lastReportMs = 0;
uint32_t uartFramesReceived = 0;
uint32_t uartAcksSent = 0;
uint32_t uartBadLines = 0;
uint32_t uartOverflowCount = 0;
char uartBuffer[96];
size_t uartLength = 0;

const CRGB LANE_COLORS[4] = {
    CRGB(32, 24, 0),
    CRGB(32, 0, 32),
    CRGB(0, 32, 32),
    CRGB(24, 24, 24),
};

void printBuildIdentity() {
  Serial.println();
  Serial.println("=== LED TWIN ECLAIR ORDINARY FASTLED/RMT PROOF ===");
  Serial.printf("FastLED version macro: %lu (expected 3010004 for 3.10.4)\n",
                static_cast<unsigned long>(FASTLED_VERSION));
  Serial.printf("Arduino-ESP32: %d.%d.%d (preferred 3.3.10)\n",
                ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR,
                ESP_ARDUINO_VERSION_PATCH);
  Serial.printf("ESP-IDF: %s\n", esp_get_idf_version());
  Serial.println("Backend requested: RMT4 (FASTLED_RMT5=0)");
  Serial.println("RMT settings: 48 words, 1 block/worker, max 4 TX workers");
  Serial.println("Controllers requested: 4 on GPIO4,5,6,7");
}

void registerControllers() {
  FastLED.addLeds<WS2812B, 4, GRB>(z4, Z4_COUNT);
  Serial.printf("REGISTERED software controller Z4 GPIO4 pixels=%u\n", Z4_COUNT);

  FastLED.addLeds<WS2812B, 5, GRB>(z5, Z5_COUNT);
  Serial.printf("REGISTERED software controller Z5 GPIO5 pixels=%u\n", Z5_COUNT);

  FastLED.addLeds<WS2812B, 6, GRB>(z6, Z6_COUNT);
  Serial.printf("REGISTERED software controller Z6 GPIO6 pixels=%u\n", Z6_COUNT);

  FastLED.addLeds<WS2812B, 7, GRB>(z7, Z7_COUNT);
  Serial.printf("REGISTERED software controller Z7 GPIO7 pixels=%u\n", Z7_COUNT);

  FastLED.setBrightness(TEST_BRIGHTNESS);
  FastLED.setDither(0);
  FastLED.clear(false);
}

void renderLane(CRGB *leds,
                uint16_t count,
                uint8_t laneIndex,
                uint32_t frame,
                uint8_t mode) {
  switch (mode) {
    case 0:
      fill_solid(leds, count, LANE_COLORS[laneIndex]);
      break;

    case 1: {
      fill_solid(leds, count, CRGB::Black);
      const uint16_t marker = static_cast<uint16_t>((frame * 5 + laneIndex * 29) % count);
      leds[marker] = CRGB::White;
      if (marker > 0) leds[marker - 1] = LANE_COLORS[laneIndex];
      if (marker + 1 < count) leds[marker + 1] = LANE_COLORS[laneIndex];
      break;
    }

    case 2:
      for (uint16_t i = 0; i < count; i++) {
        leds[i] = ((i + frame + laneIndex) & 1U) ? CRGB::White : CRGB::Black;
      }
      break;

    case 3:
      fill_rainbow(leds,
                   count,
                   static_cast<uint8_t>(frame * 2 + laneIndex * 41),
                   3);
      break;

    default:
      for (uint16_t i = 0; i < count; i++) {
        const uint8_t phase = static_cast<uint8_t>((i + frame * 3 + laneIndex * 23) % 48);
        leds[i] = phase < 16 ? CRGB(48, 0, 0)
                            : (phase < 32 ? CRGB(0, 48, 0) : CRGB(0, 0, 48));
      }
      break;
  }
}

void renderFrame(uint32_t frame, uint8_t mode) {
  renderLane(z4, Z4_COUNT, 0, frame, mode);
  renderLane(z5, Z5_COUNT, 1, frame, mode);
  renderLane(z6, Z6_COUNT, 2, frame, mode);
  renderLane(z7, Z7_COUNT, 3, frame, mode);
}

uint32_t measuredShow() {
  const uint32_t startedUs = micros();
  FastLED.show();
  const uint32_t elapsedUs = micros() - startedUs;
  showStats.add(elapsedUs);
  return elapsedUs;
}

void handleUartLine(const char *line) {
  unsigned long receivedFrame = 0;
  unsigned int receivedMode = 0;

  if (sscanf(line, "F,%lu,%u", &receivedFrame, &receivedMode) == 2) {
    commandedFrameNumber = static_cast<uint32_t>(receivedFrame);
    commandedMode = static_cast<uint8_t>(receivedMode % 5U);
    lastCommandMs = millis();
    uartFramesReceived++;

    linkSerial.printf("A,%lu\n", receivedFrame);
    uartAcksSent++;
    return;
  }

  uartBadLines++;
  Serial.printf("UART unexpected: %s\n", line);
}

void serviceUart() {
  while (linkSerial.available() > 0) {
    const char c = static_cast<char>(linkSerial.read());
    if (c == '\r') continue;

    if (c == '\n') {
      uartBuffer[uartLength] = '\0';
      if (uartLength > 0) handleUartLine(uartBuffer);
      uartLength = 0;
      continue;
    }

    if (uartLength + 1 < sizeof(uartBuffer)) {
      uartBuffer[uartLength++] = c;
    } else {
      uartLength = 0;
      uartOverflowCount++;
    }
  }
}

void printRollingReport() {
  const uint32_t nowMs = millis();
  if (nowMs - lastReportMs < REPORT_INTERVAL_MS) return;
  lastReportMs = nowMs;

  const bool linked = (nowMs - lastCommandMs) <= LINK_FRESH_MS;
  Serial.printf(
      "REPORT local_frames=%lu source=%s commanded_frame=%lu show_returned=%lu "
      "show_us[min=%lu avg=%lu max=%lu] show_stalls=%lu uart_rx=%lu "
      "uart_ack=%lu uart_bad=%lu uart_overflow=%lu heap=%lu\n",
      static_cast<unsigned long>(localFrameNumber),
      linked ? "TARDI" : "LOCAL",
      static_cast<unsigned long>(commandedFrameNumber),
      static_cast<unsigned long>(showStats.samples),
      static_cast<unsigned long>(showStats.printableMinimumUs()),
      static_cast<unsigned long>(showStats.averageUs()),
      static_cast<unsigned long>(showStats.maximumUs),
      static_cast<unsigned long>(showStats.stallCount),
      static_cast<unsigned long>(uartFramesReceived),
      static_cast<unsigned long>(uartAcksSent),
      static_cast<unsigned long>(uartBadLines),
      static_cast<unsigned long>(uartOverflowCount),
      static_cast<unsigned long>(ESP.getFreeHeap()));
}

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);
  printBuildIdentity();

  linkSerial.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  Serial.printf("UART1 started: RX=GPIO%d TX=GPIO%d baud=%lu\n",
                LINK_RX_PIN,
                LINK_TX_PIN,
                static_cast<unsigned long>(LINK_BAUD));

  registerControllers();

  Serial.println("FIRST SHOW: sending black frame...");
  const uint32_t firstShowUs = measuredShow();
  Serial.printf("FIRST SHOW RETURNED in %lu us. Check Serial for RMT errors.\n",
                static_cast<unsigned long>(firstShowUs));
  Serial.println("PHYSICAL OUTPUT NOT YET PROVEN: observe Z4-Z7.");

  nextFrameUs = micros();
  lastReportMs = millis();
}

void loop() {
  serviceUart();

  const uint32_t nowUs = micros();
  if (static_cast<int32_t>(nowUs - nextFrameUs) >= 0) {
    nextFrameUs += FRAME_INTERVAL_US;
    if (static_cast<int32_t>(nowUs - nextFrameUs) >
        static_cast<int32_t>(FRAME_INTERVAL_US * 4UL)) {
      nextFrameUs = nowUs + FRAME_INTERVAL_US;
    }

    const uint32_t nowMs = millis();
    const bool linked = (nowMs - lastCommandMs) <= LINK_FRESH_MS;
    const uint32_t frame = linked ? commandedFrameNumber : localFrameNumber;
    const uint8_t mode = linked
                             ? commandedMode
                             : static_cast<uint8_t>((nowMs / 5000UL) % 5UL);

    renderFrame(frame, mode);
    measuredShow();
    localFrameNumber++;
  }

  printRollingReport();
  delay(0);
}
