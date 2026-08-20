// =============================================================================
// LCD DRIVER TEST — 8 parallel WS2812 lanes on the ESP32-S3
// =============================================================================
//
// Bring-up tool. Run this FIRST on any new Eclair board, before flashing the
// real firmware. It answers three questions and nothing else:
//
//   1. Does the LCD (LCD_CAM / I80) driver actually get selected, or does
//      FastLED quietly fall back to RMT?
//   2. Do all EIGHT lanes come up on the production pins?
//   3. Does the DMA buffer allocate at 400 px on the longest lane?
//
// Question 3 was the one real risk in the direct-drive plan. It passed.
//
// -----------------------------------------------------------------------------
// WHY THIS SKETCH EXISTS
// -----------------------------------------------------------------------------
//
// The first version of this test used:
//
//     #define FASTLED_ESP32S3_LCD_DRIVER      // not a real macro
//     FastLED.addLeds<WS2812, PIN, GRB>(...)  // always routes to RMT
//
// Two problems. The macro name was wrong (it is FASTLED_ESP32_LCD_DRIVER, no
// S3), and a #define in a .ino never reaches FastLED anyway because the IDE
// compiles libraries as separate translation units. So the test ran on RMT —
// the exact 4-channel peripheral it was trying to escape — and "4 lanes work"
// looked like a result when it was really just the limit being hit.
//
// On this version of FastLED, addLeds<> ALWAYS lands on RMT. The Channel API
// below, naming the bus explicitly, is what actually selects LCD.
//
// -----------------------------------------------------------------------------
// HOW TO READ THE RESULT
// -----------------------------------------------------------------------------
//
// Two independent checks that have to agree:
//
//   driver column — FastLED reporting what it actually did. This is proof.
//   frame time    — physics. 400 px x 24 bits x 1.25 us = 12.0 ms if the lanes
//                   went out together. Roughly double means two sequential
//                   batches, i.e. RMT.
//
// SAFE TO RUN WITH NO LEDS ATTACHED. Every diagnostic here is meaningful on a
// bare board, and that is the best way to run it first.
//
// -----------------------------------------------------------------------------
// ARDUINO IDE
// -----------------------------------------------------------------------------
//   Board            : ESP32S3 Dev Module
//   USB CDC On Boot  : ENABLED   <-- required, GPIO43 is lane Z7
//   PSRAM            : OPI PSRAM
//   Plug into the USB port, not the UART port.
//   build_opt.h must appear as a second tab, or the flags miss the library.
// =============================================================================

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
// REQUIRED. Carries the BusTraits specialisation for LCD_CLOCKLESS. Without
// it: "error: incomplete type 'fl::BusTraits<fl::Bus::LCD_CLOCKLESS>'",
// because only the RMT specialisation is pulled in automatically.
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"

#include <esp_heap_caps.h>

// --- production lane map (must match led_direct_output.cpp) ---
const int LANES = 8;
const int ledPins[LANES]     = {   1,   2,  39,  40,  41,  42,  43,  38 };
const int ledCounts[LANES]   = { 208, 325, 400, 300, 300, 300,  75, 100 };
const char *zoneNames[LANES] = { "Z1 mouth", "Z2 shoulder", "Z3 midbody",
                                 "Z4 rear", "Z5 frontlegs", "Z6 backlegs",
                                 "Z7 digestive", "Z8 stations" };

// GPIO0 is claimed by the LCD driver as an internal padding pin. Leave it
// unwired. Documented driver behaviour, not a fault.

const int TOTAL = 208 + 325 + 400 + 300 + 300 + 300 + 75 + 100;   // 2008
const int LONGEST = 400;                                          // sets frame time

CRGB leds[TOTAL];
int laneStart[LANES];

const CRGB palette[7] = {
  CRGB(255,0,0), CRGB(255,124,0), CRGB(255,230,0), CRGB(0,255,0),
  CRGB(0,0,255), CRGB(150,0,255), CRGB(0,0,0)
};

unsigned int counter = 0;
char driverName[LANES][24];
int  seen = 0;

void onChannelEnqueued(const fl::IChannel &channel, const fl::string &name) {
  if (seen < LANES) {
    strncpy(driverName[seen], name.c_str(), sizeof(driverName[0]) - 1);
    driverName[seen][sizeof(driverName[0]) - 1] = 0;
  }
  seen++;
}

void printHeap(const char *phase) {
  Serial.printf("HEAP %-9s int8=%7u (max %7u)  dma=%7u (max %7u)  psram=%8u\n",
    phase,
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
    heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

void setup() {
  Serial.begin(115200);
  delay(2000);                       // let native USB CDC enumerate

  for (int i = 0; i < LANES; i++) strcpy(driverName[i], "-");

  int offset = 0;
  for (int i = 0; i < LANES; i++) {
    laneStart[i] = offset;
    offset += ledCounts[i];
  }

  Serial.println();
  Serial.println("LCD driver test - 8 lanes, production pins");
  printHeap("pre-init");

  FastLED.channelEvents().onChannelEnqueued.add(onChannelEnqueued);

  // THIS is what selects the LCD peripheral.
  fl::enableDrivers<fl::Bus::LCD_CLOCKLESS>();

  const auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
  fl::ChannelOptions options;
  options.mBus = fl::Bus::LCD_CLOCKLESS;

  for (int i = 0; i < LANES; i++) {
    fl::ChannelConfig cfg(fl::ClocklessChipset(ledPins[i], timing),
                          fl::span<CRGB>(leds + laneStart[i], ledCounts[i]),
                          GRB, options);
    FastLED.add(fl::Channel::create(cfg));
  }

  FastLED.setBrightness(12);                          // deliberately dim
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);    // bench-safe cap

  printHeap("post-init");
}

void loop() {
  for (int i = 0; i < LANES; i++) {
    for (int p = 0; p < ledCounts[i]; p++) {
      leds[laneStart[i] + p] = palette[(p + counter) % 7];
    }
  }

  unsigned long t0 = micros();
  FastLED.show();
  unsigned long us = micros() - t0;
  counter++;

  static bool firstShow = true;
  if (firstShow) {
    firstShow = false;
    printHeap("post-show");
  }

  // --- banner every 5s, so it is on screen whenever you open the monitor ---
  static unsigned long windowStart = 0;
  static unsigned int  frames = 0;
  static unsigned long showAccum = 0;

  frames++;
  showAccum += us;
  if (windowStart == 0) windowStart = millis();

  unsigned long elapsed = millis() - windowStart;
  if (elapsed >= 5000) {
    int onLcd = 0;
    for (int i = 0; i < LANES; i++)
      if (strcmp(driverName[i], "LCD_CLOCKLESS") == 0) onLcd++;

    float transmitMs = LONGEST * 24 * 1.25 / 1000.0;
    float avgShowMs  = (showAccum / (float)frames) / 1000.0;

    Serial.println();
    Serial.println("=== LCD DRIVER TEST =====================");
    Serial.println("lane  zone           gpio  leds  driver");
    for (int i = 0; i < LANES; i++)
      Serial.printf("  %d   %-13s %4d  %4d  %s\n",
                    i, zoneNames[i], ledPins[i], ledCounts[i], driverName[i]);

    Serial.println();
    Serial.printf("driver : %d/%d on LCD_CLOCKLESS\n", onLcd, LANES);
    Serial.printf("pixels : %d logical, %d sent (lanes pad to longest, %d)\n",
                  TOTAL, LONGEST * LANES, LONGEST);
    Serial.printf("show   : %.1f ms  =  ~%.1f ms on the wire + ~%.1f ms prep\n",
                  avgShowMs, transmitMs, avgShowMs - transmitMs);
    Serial.printf("rate   : %.1f fps\n", frames * 1000.0 / elapsed);
    Serial.printf("RESULT : %s\n",
                  (onLcd == LANES) ? "PASS - all lanes parallel, direct drive works"
                                   : "FAIL - see driver column");
    if (onLcd != LANES) {
      Serial.println("         RMT there means the bus request was ignored.");
      Serial.println("         Check build_opt.h is a tab, and the lcd_spi include.");
    }
    Serial.println("=========================================");

    windowStart = millis();
    frames = 0;
    showAccum = 0;
  }

  delay(30);
}
