// LED Twin Tardi ordinary FastLED/RMT proof
// First-flash baseline:
//   ESP32 Arduino core 3.3.10
//   FastLED 3.10.4
//   ESP32-S3-DevKitC-1 / WROOM-1-N8R8
//
// Tardi drives Z1-Z3, keeps the real button/FIRE pin ownership, starts a Wi-Fi
// access point and web server, and sends frame commands to Éclair over UART.
// FIRE outputs are intentionally held HIGH/idle in this proof sketch.

#define FASTLED_RMT5 0
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL 48
#define FASTLED_RMT_MEM_BLOCKS 1
#define FASTLED_RMT_MAX_CHANNELS 4
#define FASTLED_RMT_SERIAL_DEBUG 1
#define FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS 1000
#define FASTLED_ESP32_ENABLE_LOGGING 1

#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_arduino_version.h>
#include <esp_idf_version.h>
#include <esp_system.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This proof sketch is for ESP32-S3 only."
#endif

namespace {

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t LINK_BAUD = 2000000;
constexpr int LINK_RX_PIN = 41;
constexpr int LINK_TX_PIN = 40;
constexpr uint8_t TEST_BRIGHTNESS = 32;
constexpr uint32_t FRAME_INTERVAL_US = 33333;
constexpr uint32_t STALL_THRESHOLD_US = 50000;
constexpr uint32_t REPORT_INTERVAL_MS = 2000;

constexpr char AP_SSID[] = "TARDI-TWIN-PROOF";
constexpr char AP_PASSWORD[] = "tardiproof";

constexpr uint16_t Z1_COUNT = 208;
constexpr uint16_t Z2_COUNT = 325;
constexpr uint16_t Z3_COUNT = 400;
constexpr uint16_t LOCAL_TOTAL_COUNT = Z1_COUNT + Z2_COUNT + Z3_COUNT;
static_assert(LOCAL_TOTAL_COUNT == 933, "Tardi LED Twin total must be 933");

constexpr int BUTTON_PINS[8] = {4, 5, 6, 7, 15, 16, 17, 18};
constexpr int FIRE_PINS[9] = {8, 9, 10, 11, 12, 13, 14, 21, 47};

CRGB z1[Z1_COUNT];
CRGB z2[Z2_COUNT];
CRGB z3[Z3_COUNT];

HardwareSerial linkSerial(1);
WebServer server(80);

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
uint32_t webRequestCount = 0;
uint32_t uartFramesSent = 0;
uint32_t uartAcksReceived = 0;
uint32_t uartBadLines = 0;
uint32_t uartOverflowCount = 0;
uint32_t lastAckFrame = 0;
uint8_t buttonMask = 0;
char uartBuffer[96];
size_t uartLength = 0;

const CRGB LANE_COLORS[3] = {
    CRGB(48, 0, 0),
    CRGB(0, 48, 0),
    CRGB(0, 0, 48),
};

const char INDEX_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Tardi LED Twin proof</title>
<style>body{font-family:monospace;margin:2rem;max-width:60rem}pre{white-space:pre-wrap}</style>
</head>
<body>
<h1>Tardi LED Twin proof</h1>
<p>This page requests status ten times per second to create representative AP/web load.</p>
<pre id="status">connecting...</pre>
<script>
async function poll(){
  try {
    const response = await fetch('/status?cache=' + Date.now(), {cache:'no-store'});
    document.getElementById('status').textContent = await response.text();
  } catch (error) {
    document.getElementById('status').textContent = String(error);
  }
}
setInterval(poll, 100);
poll();
</script>
</body>
</html>
)HTML";

void holdFireOutputsIdle() {
  for (int pin : FIRE_PINS) {
    digitalWrite(pin, HIGH);
  }
}

uint8_t readButtonMask() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(BUTTON_PINS[i]) == HIGH) mask |= static_cast<uint8_t>(1U << i);
  }
  return mask;
}

void printBuildIdentity() {
  Serial.println();
  Serial.println("=== LED TWIN TARDI ORDINARY FASTLED/RMT PROOF ===");
  Serial.printf("FastLED version macro: %lu (expected 3010004 for 3.10.4)\n",
                static_cast<unsigned long>(FASTLED_VERSION));
  Serial.printf("Arduino-ESP32: %d.%d.%d (preferred 3.3.10)\n",
                ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR,
                ESP_ARDUINO_VERSION_PATCH);
  Serial.printf("ESP-IDF: %s\n", esp_get_idf_version());
  Serial.println("Backend requested: RMT4 (FASTLED_RMT5=0)");
  Serial.println("RMT settings: 48 words, 1 block/worker, max 4 TX workers");
  Serial.println("Controllers requested: 3 on GPIO1,2,39");
  Serial.println("FIRE GPIOs are present but held HIGH/idle; this sketch never fires them.");
}

void configureProjectPins() {
  for (int pin : BUTTON_PINS) {
    pinMode(pin, INPUT);  // Matches final external 10k pull-down wiring.
  }

  for (int pin : FIRE_PINS) {
    digitalWrite(pin, HIGH);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

void registerControllers() {
  FastLED.addLeds<WS2812B, 1, GRB>(z1, Z1_COUNT);
  Serial.printf("REGISTERED software controller Z1 GPIO1 pixels=%u\n", Z1_COUNT);

  FastLED.addLeds<WS2812B, 2, GRB>(z2, Z2_COUNT);
  Serial.printf("REGISTERED software controller Z2 GPIO2 pixels=%u\n", Z2_COUNT);

  FastLED.addLeds<WS2812B, 39, GRB>(z3, Z3_COUNT);
  Serial.printf("REGISTERED software controller Z3 GPIO39 pixels=%u\n", Z3_COUNT);

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
  renderLane(z1, Z1_COUNT, 0, frame, mode);
  renderLane(z2, Z2_COUNT, 1, frame, mode);
  renderLane(z3, Z3_COUNT, 2, frame, mode);
}

uint32_t measuredShow() {
  const uint32_t startedUs = micros();
  FastLED.show();
  const uint32_t elapsedUs = micros() - startedUs;
  showStats.add(elapsedUs);
  return elapsedUs;
}

String makeStatusJson() {
  String json;
  json.reserve(384);
  json += '{';
  json += "\"role\":\"Tardi\",";
  json += "\"frames\":"; json += String(frameNumber); json += ',';
  json += "\"show_min_us\":"; json += String(showStats.printableMinimumUs()); json += ',';
  json += "\"show_avg_us\":"; json += String(showStats.averageUs()); json += ',';
  json += "\"show_max_us\":"; json += String(showStats.maximumUs); json += ',';
  json += "\"show_stalls\":"; json += String(showStats.stallCount); json += ',';
  json += "\"web_requests\":"; json += String(webRequestCount); json += ',';
  json += "\"wifi_clients\":"; json += String(WiFi.softAPgetStationNum()); json += ',';
  json += "\"uart_sent\":"; json += String(uartFramesSent); json += ',';
  json += "\"uart_acks\":"; json += String(uartAcksReceived); json += ',';
  json += "\"last_ack_frame\":"; json += String(lastAckFrame); json += ',';
  json += "\"uart_bad\":"; json += String(uartBadLines); json += ',';
  json += "\"button_mask\":"; json += String(buttonMask); json += ',';
  json += "\"heap\":"; json += String(ESP.getFreeHeap());
  json += '}';
  return json;
}

void startWebLoad() {
  WiFi.mode(WIFI_AP);
  const bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("Wi-Fi AP start: %s\n", apStarted ? "OK" : "FAILED");
  Serial.printf("AP SSID: %s password: %s IP: %s\n",
                AP_SSID,
                AP_PASSWORD,
                WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, []() {
    webRequestCount++;
    server.send_P(200, "text/html", INDEX_PAGE);
  });

  server.on("/status", HTTP_GET, []() {
    webRequestCount++;
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", makeStatusJson());
  });

  server.onNotFound([]() {
    webRequestCount++;
    server.send(404, "text/plain", "not found");
  });

  server.begin();
  Serial.println("Web server started. Connect a phone/laptop and leave 192.168.4.1 open.");
}

void handleUartLine(const char *line) {
  unsigned long acknowledgedFrame = 0;
  if (sscanf(line, "A,%lu", &acknowledgedFrame) == 1) {
    uartAcksReceived++;
    lastAckFrame = static_cast<uint32_t>(acknowledgedFrame);
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

  Serial.printf(
      "REPORT frames=%lu show_returned=%lu show_us[min=%lu avg=%lu max=%lu] "
      "show_stalls=%lu wifi_clients=%u web_requests=%lu uart_sent=%lu "
      "uart_acks=%lu uart_bad=%lu uart_overflow=%lu ack_lag=%lu buttons=0x%02X heap=%lu\n",
      static_cast<unsigned long>(frameNumber),
      static_cast<unsigned long>(showStats.samples),
      static_cast<unsigned long>(showStats.printableMinimumUs()),
      static_cast<unsigned long>(showStats.averageUs()),
      static_cast<unsigned long>(showStats.maximumUs),
      static_cast<unsigned long>(showStats.stallCount),
      static_cast<unsigned>(WiFi.softAPgetStationNum()),
      static_cast<unsigned long>(webRequestCount),
      static_cast<unsigned long>(uartFramesSent),
      static_cast<unsigned long>(uartAcksReceived),
      static_cast<unsigned long>(uartBadLines),
      static_cast<unsigned long>(uartOverflowCount),
      static_cast<unsigned long>(frameNumber - lastAckFrame),
      buttonMask,
      static_cast<unsigned long>(ESP.getFreeHeap()));
}

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);
  printBuildIdentity();

  configureProjectPins();

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
  Serial.println("PHYSICAL OUTPUT NOT YET PROVEN: observe Z1-Z3.");

  startWebLoad();

  nextFrameUs = micros();
  lastReportMs = millis();
}

void loop() {
  server.handleClient();
  serviceUart();
  buttonMask = readButtonMask();
  holdFireOutputsIdle();

  const uint32_t nowUs = micros();
  if (static_cast<int32_t>(nowUs - nextFrameUs) >= 0) {
    nextFrameUs += FRAME_INTERVAL_US;
    if (static_cast<int32_t>(nowUs - nextFrameUs) >
        static_cast<int32_t>(FRAME_INTERVAL_US * 4UL)) {
      nextFrameUs = nowUs + FRAME_INTERVAL_US;
    }

    const uint8_t mode = static_cast<uint8_t>((millis() / 5000UL) % 5UL);

    // Send the intended remote frame first, then draw the local half.
    linkSerial.printf("F,%lu,%u\n",
                      static_cast<unsigned long>(frameNumber),
                      static_cast<unsigned>(mode));
    uartFramesSent++;

    renderFrame(frameNumber, mode);
    measuredShow();
    frameNumber++;
  }

  printRollingReport();
  delay(0);
}
