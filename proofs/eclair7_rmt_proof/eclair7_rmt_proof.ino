// Éclair 7 ordinary FastLED/RMT proof
// First-flash baseline:
//   ESP32 Arduino core 3.3.10
//   FastLED 3.10.4
//   ESP32-S3-DevKitC-1 / WROOM-1-N8R8
//
// This deliberately forces FastLED's RMT4 scheduler on ESP-IDF 5.
// Seven controllers are registered; up to four S3 RMT TX workers are
// used concurrently and the remaining controllers are time-multiplexed.

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
constexpr uint32_t FRAME_INTERVAL_US = 33333;  // 30 complete updates/second.
constexpr uint32_t STALL_THRESHOLD_US = 50000;
constexpr uint32_t REPORT_INTERVAL_MS = 2000;

constexpr uint16_t Z1_COUNT = 208;
constexpr uint16_t Z2_COUNT = 325;
constexpr uint16_t Z3_COUNT = 400;
constexpr uint16_t Z4_COUNT = 300;
constexpr uint16_t Z5_COUNT = 300;
constexpr uint16_t Z6_COUNT = 300;
constexpr uint16_t Z7_COUNT = 75;
constexpr uint16_t TOTAL_COUNT =
    Z1_COUNT + Z2_COUNT + Z3_COUNT + Z4_COUNT + Z5_COUNT + Z6_COUNT + Z7_COUNT;

static_assert(TOTAL_COUNT == 1908, "Éclair 7 pixel total must be 1908");

CRGB z1[Z1_COUNT];
CRGB z2[Z2_COUNT];
CRGB z3[Z3_COUNT];
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
uint32_t frameNumber = 0;
uint32_t nextFrameUs = 0;
uint32_t lastReportMs = 0;
uint32_t uartRxLines = 0;
uint32_t uartOverflowCount = 0;
char uartBuffer[128];
size_t uartLength = 0;

const CRGB LANE_COLORS[7] = {
    CRGB(48, 0, 0),
    CRGB(0, 48, 0),
    CRGB(0, 0, 48),
    CRGB(32, 24, 0),
    CRGB(32, 0, 32),
    CRGB(0, 32, 32),
    CRGB(24, 24, 24),
};

void printBuildIdentity() {
  Serial.println();
  Serial.println("=== ECLAIR 7 ORDINARY FASTLED/RMT PROOF ===");
  Serial.printf("FastLED version macro: %lu (expected 3010004 for 3.10.4)\n",
                static_cast<unsigned long>(FASTLED_VERSION));
  Serial.printf("Arduino-ESP32: %d.%d.%d (preferred 3.3.10)\n",
                ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR,
                ESP_ARDUINO_VERSION_PATCH);
  Serial.printf("ESP-IDF: %s\n", esp_get_idf_version());
  Serial.println("Backend requested: RMT4 (FASTLED_RMT5=0)");
  Serial.println("RMT settings: 48 words, 1 block/worker, max 4 TX workers");
  Serial.println("Controllers requested: 7 on GPIO4,5,6,7,8,9,10");
  Serial.println("Registration messages below prove software registration only.");
  Serial.println("A returned FastLED.show() proves the call completed, not that LEDs received it.");
}

void registerControllers() {
  FastLED.addLeds<WS2812B, 4, GRB>(z1, Z1_COUNT);
  Serial.printf("REGISTERED software controller Z1 GPIO4 pixels=%u\n", Z1_COUNT);

  FastLED.addLeds<WS2812B, 5, GRB>(z2, Z2_COUNT);
  Serial.printf("REGISTERED software controller Z2 GPIO5 pixels=%u\n", Z2_COUNT);

  FastLED.addLeds<WS2812B, 6, GRB>(z3, Z3_COUNT);
  Serial.printf("REGISTERED software controller Z3 GPIO6 pixels=%u\n", Z3_COUNT);

  FastLED.addLeds<WS2812B, 7, GRB>(z4, Z4_COUNT);
  Serial.printf("REGISTERED software controller Z4 GPIO7 pixels=%u\n", Z4_COUNT);

  FastLED.addLeds<WS2812B, 8, GRB>(z5, Z5_COUNT);
  Serial.printf("REGISTERED software controller Z5 GPIO8 pixels=%u\n", Z5_COUNT);

  FastLED.addLeds<WS2812B, 9, GRB>(z6, Z6_COUNT);
  Serial.printf("REGISTERED software controller Z6 GPIO9 pixels=%u\n", Z6_COUNT);

  FastLED.addLeds<WS2812B, 10, GRB>(z7, Z7_COUNT);
  Serial.printf("REGISTERED software controller Z7 GPIO10 pixels=%u\n", Z7_COUNT);

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
    case 0: {
      fill_solid(leds, count, LANE_COLORS[laneIndex]);
      break;
    }

    case 1: {
      fill_solid(leds, count, CRGB::Black);
      const uint16_t marker = static_cast<uint16_t>((frame * 5 + laneIndex * 17) % count);
      leds[marker] = CRGB::White;
      if (marker > 0) leds[marker - 1] = LANE_COLORS[laneIndex];
      if (marker + 1 < count) leds[marker + 1] = LANE_COLORS[laneIndex];
      break;
    }

    case 2: {
      for (uint16_t i = 0; i < count; i++) {
        const bool on = ((i + frame + laneIndex) & 1U) != 0;
        leds[i] = on ? CRGB::White : CRGB::Black;
      }
      break;
    }

    case 3: {
      fill_rainbow(leds,
                   count,
                   static_cast<uint8_t>(frame * 2 + laneIndex * 31),
                   3);
      break;
    }

    default: {
      for (uint16_t i = 0; i < count; i++) {
        const uint8_t phase = static_cast<uint8_t>((i + frame * 3 + laneIndex * 23) % 48);
        if (phase < 16) {
          leds[i] = CRGB(48, 0, 0);
        } else if (phase < 32) {
          leds[i] = CRGB(0, 48, 0);
        } else {
          leds[i] = CRGB(0, 0, 48);
        }
      }
      break;
    }
  }
}

void renderFrame(uint32_t frame) {
  const uint8_t mode = static_cast<uint8_t>((millis() / 5000UL) % 5UL);
  renderLane(z1, Z1_COUNT, 0, frame, mode);
  renderLane(z2, Z2_COUNT, 1, frame, mode);
  renderLane(z3, Z3_COUNT, 2, frame, mode);
  renderLane(z4, Z4_COUNT, 3, frame, mode);
  renderLane(z5, Z5_COUNT, 4, frame, mode);
  renderLane(z6, Z6_COUNT, 5, frame, mode);
  renderLane(z7, Z7_COUNT, 6, frame, mode);
}

uint32_t measuredShow() {
  const uint32_t startedUs = micros();
  FastLED.show();
  const uint32_t elapsedUs = micros() - startedUs;
  showStats.add(elapsedUs);
  return elapsedUs;
}

void handleUartLine(const char *line) {
  uartRxLines++;
  Serial.printf("UART RX: %s\n", line);
  linkSerial.printf("E7_ACK,%lu,%s\n",
                    static_cast<unsigned long>(frameNumber),
                    line);
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

  Serial.printf(
      "REPORT frames=%lu show_returned=%lu show_us[min=%lu avg=%lu max=%lu] "
      "show_stalls=%lu uart_rx=%lu uart_overflow=%lu heap=%lu mode=%lu\n",
      static_cast<unsigned long>(frameNumber),
      static_cast<unsigned long>(showStats.samples),
      static_cast<unsigned long>(showStats.printableMinimumUs()),
      static_cast<unsigned long>(showStats.averageUs()),
      static_cast<unsigned long>(showStats.maximumUs),
      static_cast<unsigned long>(showStats.stallCount),
      static_cast<unsigned long>(uartRxLines),
      static_cast<unsigned long>(uartOverflowCount),
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>((nowMs / 5000UL) % 5UL));

  linkSerial.printf(
      "E7_STATUS,%lu,%lu,%lu,%lu,%lu\n",
      static_cast<unsigned long>(frameNumber),
      static_cast<unsigned long>(showStats.averageUs()),
      static_cast<unsigned long>(showStats.maximumUs),
      static_cast<unsigned long>(showStats.stallCount),
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
  Serial.println("PHYSICAL OUTPUT NOT YET PROVEN: observe all seven connected lanes.");

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

    renderFrame(frameNumber);
    measuredShow();
    frameNumber++;
  }

  printRollingReport();
  delay(0);
}
