// =============================================================================
// Tardi — 7-lane LCD parallel driver validation test
// =============================================================================
// Answers three questions the previous test could not:
//
//   1. Does the LCD (LCD_CAM / I80) driver actually get selected, or does
//      FastLED silently fall back to RMT?
//   2. Do all SEVEN lanes work on the PRODUCTION pins?
//   3. Does the DMA buffer allocate at 400 px on the longest lane?
//
// The previous sketch used `#define FASTLED_ESP32S3_LCD_DRIVER`, which is not
// a real FastLED macro (the correct one is FASTLED_ESP32_LCD_DRIVER) and would
// not have reached the library from the .ino anyway. It therefore ran on RMT —
// the exact 4-channel peripheral it was trying to escape.
//
// The macro now lives in build_opt.h, which IS passed to the library build.
//
// SAFE TO RUN WITH NO LEDS ATTACHED. The driver / heap / timing diagnostics
// are all meaningful with nothing connected.
// =============================================================================

// Belt and braces — the authoritative definition is in build_opt.h
#ifndef FASTLED_ESP32_LCD_DRIVER
#define FASTLED_ESP32_LCD_DRIVER
#endif

#include <FastLED.h>
#include <esp_heap_caps.h>

// --- PRODUCTION PIN MAP (docs/gpio_schema.md) --------------------------------
// Pins must be compile-time constants for FastLED's template API.
#define PIN_Z1   1     // mouth
#define PIN_Z2   2     // shoulder
#define PIN_Z3  39     // midbody   (longest lane — the one that matters)
#define PIN_Z4  40     // rear
#define PIN_Z5  41     // front legs
#define PIN_Z6  42     // back legs
#define PIN_Z7  43     // digestive (was UART0 TX — see README notes)

#define N_Z1  208
#define N_Z2  325
#define N_Z3  400
#define N_Z4  300
#define N_Z5  300
#define N_Z6  300
#define N_Z7   75

#define OFF_Z1  0
#define OFF_Z2  (OFF_Z1 + N_Z1)   //  208
#define OFF_Z3  (OFF_Z2 + N_Z2)   //  533
#define OFF_Z4  (OFF_Z3 + N_Z3)   //  933
#define OFF_Z5  (OFF_Z4 + N_Z4)   // 1233
#define OFF_Z6  (OFF_Z5 + N_Z5)   // 1533
#define OFF_Z7  (OFF_Z6 + N_Z6)   // 1833
#define N_TOTAL (OFF_Z7 + N_Z7)   // 1908

// GPIO0 is claimed by the parallel driver as an internal padding / "ghost" pin.
// Leave it unwired. This is documented behaviour, not a bug.

// --- SAFETY ------------------------------------------------------------------
// Deliberately dim. Raise with '+' once you trust the wiring and the supply.
uint8_t  gBrightness  = 12;      // out of 255
const uint32_t POWER_LIMIT_MA = 1500;   // bench-safe cap

CRGB leds[N_TOTAL];

// --- STATE -------------------------------------------------------------------
enum Mode { MODE_TRAVEL, MODE_LANE, MODE_SOLID, MODE_OFF };
Mode     gMode      = MODE_TRAVEL;
int      gLaneOnly  = -1;        // 1..7 in MODE_LANE
CRGB     gSolid     = CRGB(255, 0, 0);
uint32_t gCounter   = 0;
bool     gFirstShow = true;

struct LaneInfo { const char *name; int pin; int offset; int count; };
const LaneInfo LANES[7] = {
  { "Z1 mouth",     PIN_Z1, OFF_Z1, N_Z1 },
  { "Z2 shoulder",  PIN_Z2, OFF_Z2, N_Z2 },
  { "Z3 midbody",   PIN_Z3, OFF_Z3, N_Z3 },
  { "Z4 rear",      PIN_Z4, OFF_Z4, N_Z4 },
  { "Z5 frontlegs", PIN_Z5, OFF_Z5, N_Z5 },
  { "Z6 backlegs",  PIN_Z6, OFF_Z6, N_Z6 },
  { "Z7 digestive", PIN_Z7, OFF_Z7, N_Z7 },
};

// =============================================================================
// HEAP REPORTING
// =============================================================================
void printHeap(const char *phase) {
  Serial.printf(
    "HEAP %-8s int8=%7u (largest %7u)  dma=%7u (largest %7u)  psram=%8u\n",
    phase,
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
    heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(2500);            // give native USB CDC time to enumerate

  Serial.println();
  Serial.println("=====================================================");
  Serial.println(" Tardi 7-lane LCD parallel driver test");
  Serial.println("=====================================================");

#ifdef FASTLED_ESP32_LCD_DRIVER
  Serial.println("FASTLED_ESP32_LCD_DRIVER : visible in sketch");
#else
  Serial.println("FASTLED_ESP32_LCD_DRIVER : *** NOT DEFINED ***");
#endif
  Serial.printf("FastLED version          : %d\n", FASTLED_VERSION);
  Serial.printf("Total logical pixels     : %d\n", N_TOTAL);
  Serial.printf("Longest lane             : Z3 @ %d px\n", N_Z3);
  Serial.println();

  printHeap("pre-init");

  FastLED.addLeds<WS2812, PIN_Z1, GRB>(leds + OFF_Z1, N_Z1);
  FastLED.addLeds<WS2812, PIN_Z2, GRB>(leds + OFF_Z2, N_Z2);
  FastLED.addLeds<WS2812, PIN_Z3, GRB>(leds + OFF_Z3, N_Z3);
  FastLED.addLeds<WS2812, PIN_Z4, GRB>(leds + OFF_Z4, N_Z4);
  FastLED.addLeds<WS2812, PIN_Z5, GRB>(leds + OFF_Z5, N_Z5);
  FastLED.addLeds<WS2812, PIN_Z6, GRB>(leds + OFF_Z6, N_Z6);
  FastLED.addLeds<WS2812, PIN_Z7, GRB>(leds + OFF_Z7, N_Z7);

  FastLED.setBrightness(gBrightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, POWER_LIMIT_MA);

  printHeap("post-init");

  Serial.println();
  Serial.println("Lane map:");
  for (int i = 0; i < 7; i++) {
    Serial.printf("  lane %d  %-13s GPIO%-3d  %4d px  @ index %4d\n",
                  i + 1, LANES[i].name, LANES[i].pin,
                  LANES[i].count, LANES[i].offset);
  }

  Serial.println();
  Serial.println("Commands:  a=travel  1-7=single lane  r/g/b=solid  0=off");
  Serial.println("           +/- brightness   h=heap   ?=help");
  Serial.println();
  Serial.println("-- first show() follows --");
}

// =============================================================================
// PATTERN
// =============================================================================
void renderTravel() {
  static const CRGB palette[7] = {
    CRGB(255,   0,   0),   // red
    CRGB(255, 124,   0),   // orange
    CRGB(255, 230,   0),   // yellow
    CRGB(  0, 255,   0),   // green
    CRGB(  0,   0, 255),   // blue
    CRGB(150,   0, 255),   // purple
    CRGB(  0,   0,   0),   // gap
  };
  // Write EVERY pixel every frame; offset the colour lookup so the pattern
  // travels. (The old sketch offset the loop START, which left the first
  // pixels holding stale data and never actually moved.)
  for (int i = 0; i < N_TOTAL; i++) {
    leds[i] = palette[(i + gCounter) % 7];
  }
}

void renderLaneOnly(int lane) {
  fill_solid(leds, N_TOTAL, CRGB::Black);
  if (lane < 1 || lane > 7) return;
  const LaneInfo &L = LANES[lane - 1];
  // Dim white, plus a red head pixel so orientation is obvious
  fill_solid(leds + L.offset, L.count, CRGB(60, 60, 60));
  leds[L.offset] = CRGB(255, 0, 0);
}

// =============================================================================
// SERIAL
// =============================================================================
void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'a': gMode = MODE_TRAVEL; Serial.println("mode: travel"); break;
      case '0': gMode = MODE_OFF;    Serial.println("mode: off");    break;
      case 'r': gMode = MODE_SOLID; gSolid = CRGB(255,0,0);
                Serial.println("solid RED   (wrong colour = wrong byte order)"); break;
      case 'g': gMode = MODE_SOLID; gSolid = CRGB(0,255,0);
                Serial.println("solid GREEN (wrong colour = wrong byte order)"); break;
      case 'b': gMode = MODE_SOLID; gSolid = CRGB(0,0,255);
                Serial.println("solid BLUE  (wrong colour = wrong byte order)"); break;
      case 'h': printHeap("now"); break;
      case '+': if (gBrightness <= 235) gBrightness += 20;
                FastLED.setBrightness(gBrightness);
                Serial.printf("brightness: %d\n", gBrightness); break;
      case '-': if (gBrightness >= 30)  gBrightness -= 20;
                FastLED.setBrightness(gBrightness);
                Serial.printf("brightness: %d\n", gBrightness); break;
      case '?': Serial.println("a=travel 1-7=lane r/g/b=solid 0=off +/- h=heap"); break;
      default:
        if (c >= '1' && c <= '7') {
          gMode = MODE_LANE;
          gLaneOnly = c - '0';
          Serial.printf("mode: lane %d only  (%s, GPIO%d, %d px)\n",
                        gLaneOnly, LANES[gLaneOnly-1].name,
                        LANES[gLaneOnly-1].pin, LANES[gLaneOnly-1].count);
        }
        break;
    }
  }
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  handleSerial();

  switch (gMode) {
    case MODE_TRAVEL: renderTravel(); break;
    case MODE_LANE:   renderLaneOnly(gLaneOnly); break;
    case MODE_SOLID:  fill_solid(leds, N_TOTAL, gSolid); break;
    case MODE_OFF:    fill_solid(leds, N_TOTAL, CRGB::Black); break;
  }

  // --- ONE show per frame (the old sketch called it once per PIXEL) ---
  uint32_t t0 = micros();
  FastLED.show();
  uint32_t dt = micros() - t0;

  if (gFirstShow) {
    gFirstShow = false;
    printHeap("post-show");
    Serial.println();
    Serial.printf("First show(): %lu us\n", (unsigned long)dt);
    Serial.println();
    Serial.println("--- INTERPRETATION -------------------------------");
    Serial.println("Longest lane is 400 px = 400 x 24 bits x 1.25us = ~12.0 ms");
    Serial.println();
    if (dt < 16000) {
      Serial.println("  ~12-16 ms  -> PARALLEL. All 7 lanes transmit at once.");
      Serial.println("                This is the LCD driver. Plan 1 is viable.");
    } else if (dt < 40000) {
      Serial.println("  ~20-30 ms  -> SEQUENTIAL / round-robin.");
      Serial.println("                Almost certainly RMT (4 HW channels on S3,");
      Serial.println("                so 7 lanes go out in two batches).");
      Serial.println("                build_opt.h did not reach the library.");
    } else {
      Serial.println("  >40 ms     -> something else is wrong. Check wiring,");
      Serial.println("                pin conflicts, and the heap numbers above.");
    }
    Serial.println("--------------------------------------------------");
    Serial.println();
  }

  // Rolling frame-time report
  static uint32_t lastReport = 0, frames = 0, sum = 0, worst = 0;
  frames++; sum += dt; if (dt > worst) worst = dt;
  if (millis() - lastReport > 3000) {
    lastReport = millis();
    Serial.printf("show(): avg %lu us   max %lu us   fps %lu   mode %d   bright %d\n",
                  (unsigned long)(sum / frames), (unsigned long)worst,
                  (unsigned long)(1000000UL / (sum / frames)),
                  (int)gMode, gBrightness);
    frames = 0; sum = 0; worst = 0;
  }

  gCounter++;
  delay(30);
}
