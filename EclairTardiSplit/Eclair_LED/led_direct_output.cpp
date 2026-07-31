#include "led_direct_output.h"

#include <FastLED.h>
#include <esp_heap_caps.h>
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
// REQUIRED. Carries the BusTraits specialisation for LCD_CLOCKLESS.
// Without it: "error: incomplete type 'fl::BusTraits<fl::Bus::LCD_CLOCKLESS>'"
// because only the RMT specialisation is pulled in automatically.
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"

#include "led_config.h"
#include "led_engine.h"
#include "led_layout.h"

// =============================================================================
// LANE MAP
// =============================================================================
//
// Eight lanes on the ESP32-S3. Lane order follows the LOGICAL zone order
// (Z1..Z8), unlike the old Output Expander which had Z8 on Ch0.
//
// GPIO0 is claimed by the LCD driver as an internal padding / "ghost" pin.
// It carries no strip and must be left unwired. This is documented driver
// behaviour, not a fault.
//
// GPIO38 is available for Z8 only because the OLED moved to the Tardi fire
// board in the two-board split. On the single-board build there was no
// eighth lane and Z8 had to be dropped.
//
// Unavailable on ESP32-S3-WROOM-1-N8R8:
//   GPIO26-32  SPI flash
//   GPIO33-37  octal PSRAM
//   GPIO19/20  native USB
//   GPIO45/46  strapping
//   GPIO48     onboard RGB LED

constexpr uint8_t LED_LANE_COUNT = 8;

struct LedLaneConfig {
  uint8_t laneId;
  uint8_t zoneIndex;
  int dataPin;
  uint16_t pixels;
  uint16_t startIndex;
  const char *name;
};

static const LedLaneConfig LED_LANES[LED_LANE_COUNT] = {
  { 0, LED_ZONE_Z1_MOUTH,       1, 208,    0, "Z1 mouth"     },
  { 1, LED_ZONE_Z2_SHOULDER,    2, 325,  208, "Z2 shoulder"  },
  { 2, LED_ZONE_Z3_MIDBODY,    39, 400,  533, "Z3 midbody"   },
  { 3, LED_ZONE_Z4_REAR,       40, 300,  933, "Z4 rear"      },
  { 4, LED_ZONE_Z5_FRONT_LEGS, 41, 300, 1233, "Z5 frontlegs" },
  { 5, LED_ZONE_Z6_BACK_LEGS,  42, 300, 1533, "Z6 backlegs"  },
  { 6, LED_ZONE_Z7_DIGESTIVE,  43,  75, 1833, "Z7 digestive" },
  { 7, LED_ZONE_Z8_STATIONS,   38, 100, 1908, "Z8 stations"  },
};

// Longest lane sets the frame time: parallel output pads every lane out to
// the longest one, so Z3 at 400 px is the floor for the whole frame.
constexpr uint16_t LED_LONGEST_LANE = 400;

// Live hardware build. Set false only for desk work with no strips attached.
constexpr bool ENABLE_REAL_LED_OUTPUT = true;

// Bench-safe current ceiling. FastLED scales the whole frame down rather than
// let the 5V rail sag. RAISE THIS ONLY AFTER the power distribution work in
// docs/power_and_ground.md is done — the sculpture is 2,358 PHYSICAL LEDs,
// not 2,008, because Z7 is seven parallel strands off one data line.
constexpr uint32_t LED_POWER_LIMIT_MA = 2000;

// =============================================================================
// STATE
// =============================================================================

static CRGB ledFrame[LED_TOTAL_PIXEL_COUNT];

static bool ledDirectInitialized = false;
static bool ledDirectStarted = false;
static LedOutputMode ledDirectRuntimeMode = LED_OUTPUT_ANIMATION;
static int ledDirectValidationLane = -1;
static LedValidationColor ledDirectValidationColor = LED_VALIDATION_COLOR_RED;

static char ledLaneDriver[LED_LANE_COUNT][24];
static uint8_t ledLanesEnqueued = 0;

static uint32_t ledLastShowMicros = 0;
static uint32_t ledShowAccum = 0;
static uint32_t ledShowSamples = 0;
static float ledAvgShowMs = 0.0f;
static float ledFps = 0.0f;
static uint32_t ledFpsWindowStart = 0;
static uint32_t ledFpsFrames = 0;

static const LedLaneConfig *ledLaneFor(uint8_t laneId) {
  for (uint8_t i = 0; i < LED_LANE_COUNT; i++) {
    if (LED_LANES[i].laneId == laneId) return &LED_LANES[i];
  }
  return nullptr;
}

static const LedLaneConfig *ledLaneForLogicalIndex(uint16_t logicalPixelIndex) {
  for (uint8_t i = 0; i < LED_LANE_COUNT; i++) {
    const LedLaneConfig &lane = LED_LANES[i];
    if (logicalPixelIndex >= lane.startIndex &&
        logicalPixelIndex < (uint16_t)(lane.startIndex + lane.pixels)) {
      return &lane;
    }
  }
  return nullptr;
}

// =============================================================================
// DRIVER REPORTING
// =============================================================================
//
// FastLED calls this once per channel on the first show() and tells us which
// driver it actually routed to. This is the ONLY reliable proof that the LCD
// bus request was honoured — the timing is corroborating evidence, not proof.

static void ledOnChannelEnqueued(const fl::IChannel &channel, const fl::string &name) {
  if (ledLanesEnqueued < LED_LANE_COUNT) {
    strncpy(ledLaneDriver[ledLanesEnqueued], name.c_str(), sizeof(ledLaneDriver[0]) - 1);
    ledLaneDriver[ledLanesEnqueued][sizeof(ledLaneDriver[0]) - 1] = 0;
  }
  ledLanesEnqueued++;
}

// =============================================================================
// VALIDATION COLOURS
// =============================================================================

static LedRgbColor ledValidationColorForLane(uint8_t laneId) {
  static const LedRgbColor colors[8] = {
    { 8, 8, 8 }, { 0, 12, 12 }, { 0, 0, 14 }, { 10, 0, 14 },
    { 14, 6, 0 }, { 14, 10, 0 }, { 14, 0, 0 }, { 12, 6, 0 }
  };
  if (laneId >= 8) return { 0, 0, 0 };
  return colors[laneId];
}

static LedRgbColor ledValidationRgbForColor(LedValidationColor color) {
  switch (color) {
    case LED_VALIDATION_COLOR_GREEN: return { 0, 12, 0 };
    case LED_VALIDATION_COLOR_BLUE:  return { 0, 0, 12 };
    case LED_VALIDATION_COLOR_RED:
    default:                         return { 12, 0, 0 };
  }
}

static LedRgbColor ledRenderRgb(uint16_t logicalPixelIndex, uint32_t nowMs) {
  if (ledDirectRuntimeMode == LED_OUTPUT_OFF) return { 0, 0, 0 };

  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_SOLID) return { 8, 8, 8 };

  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_CHANNEL) {
    const LedLaneConfig *lane = ledLaneForLogicalIndex(logicalPixelIndex);
    if (lane == nullptr || lane->laneId != ledDirectValidationLane) return { 0, 0, 0 };
    return ledValidationColorForLane(lane->laneId);
  }

  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_COLOR) {
    return ledValidationRgbForColor(ledDirectValidationColor);
  }

  LedColor hsv = ledEngineRenderPixel(logicalPixelIndex, nowMs);
  return ledColorToRgb(hsv);
}

// =============================================================================
// HEAP
// =============================================================================
//
// The LCD driver's DMA buffer must live in INTERNAL DMA-capable RAM — PSRAM
// will not do. At 6 bytes per bit and a rectangular buffer padded to the
// longest lane, that is roughly 400 px x 144 bytes ~= 58 KB. Watch
// dma_largest across the first show(); an allocation failure here is the
// most likely software-side failure of this driver.

void ledDirectOutputPrintHeap(const char *phase, Stream &out) {
  out.printf("HEAP %-9s int8=%7u (max %7u)  dma=%7u (max %7u)  psram=%8u\n",
             phase,
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

// =============================================================================
// INIT
// =============================================================================

void ledDirectOutputBegin() {
  for (uint8_t i = 0; i < LED_LANE_COUNT; i++) strcpy(ledLaneDriver[i], "-");

  fill_solid(ledFrame, LED_TOTAL_PIXEL_COUNT, CRGB::Black);

  ledDirectOutputPrintHeap("pre-init", Serial);

  FastLED.channelEvents().onChannelEnqueued.add(ledOnChannelEnqueued);

  // THIS is what selects the LCD peripheral. FastLED.addLeds<> would route
  // every lane to RMT regardless of any #define.
  fl::enableDrivers<fl::Bus::LCD_CLOCKLESS>();

  const auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
  fl::ChannelOptions options;
  options.mBus = fl::Bus::LCD_CLOCKLESS;

  for (uint8_t i = 0; i < LED_LANE_COUNT; i++) {
    const LedLaneConfig &lane = LED_LANES[i];
    fl::ChannelConfig cfg(
      fl::ClocklessChipset(lane.dataPin, timing),
      fl::span<CRGB>(ledFrame + lane.startIndex, lane.pixels),
      GRB,
      options
    );
    FastLED.add(fl::Channel::create(cfg));
  }

  FastLED.setBrightness(255);   // master dimming happens in led_settings
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_POWER_LIMIT_MA);

  ledDirectInitialized = true;
  ledDirectStarted = ENABLE_REAL_LED_OUTPUT;

  ledDirectOutputPrintHeap("post-init", Serial);
}

// =============================================================================
// FRAME
// =============================================================================

void ledDirectOutputUpdate(uint32_t nowMs) {
  if (!ENABLE_REAL_LED_OUTPUT || !ledDirectInitialized) return;
  if (ledDirectRuntimeMode == LED_OUTPUT_OFF) {
    fill_solid(ledFrame, LED_TOTAL_PIXEL_COUNT, CRGB::Black);
  } else {
    for (uint16_t i = 0; i < LED_TOTAL_PIXEL_COUNT; i++) {
      LedRgbColor rgb = ledRenderRgb(i, nowMs);
      ledFrame[i] = CRGB(rgb.r, rgb.g, rgb.b);
    }
  }

  static bool firstShow = true;

  uint32_t t0 = micros();
  FastLED.show();
  ledLastShowMicros = micros() - t0;

  ledShowAccum += ledLastShowMicros;
  ledShowSamples++;
  ledFpsFrames++;

  if (ledFpsWindowStart == 0) ledFpsWindowStart = millis();
  uint32_t windowMs = millis() - ledFpsWindowStart;
  if (windowMs >= 2000) {
    ledAvgShowMs = (ledShowAccum / (float)ledShowSamples) / 1000.0f;
    ledFps = ledFpsFrames * 1000.0f / windowMs;
    ledShowAccum = 0;
    ledShowSamples = 0;
    ledFpsFrames = 0;
    ledFpsWindowStart = millis();
  }

  if (firstShow) {
    firstShow = false;
    ledDirectOutputPrintHeap("post-show", Serial);
  }
}

// =============================================================================
// ACCESSORS
// =============================================================================

bool ledDirectOutputIsInitialized()      { return ledDirectInitialized; }
bool ledDirectOutputRealOutputAllowed()  { return ENABLE_REAL_LED_OUTPUT; }
bool ledDirectOutputRealOutputStarted()  { return ledDirectStarted; }
uint8_t ledDirectOutputConfiguredLaneCount()  { return LED_LANE_COUNT; }
uint16_t ledDirectOutputConfiguredPixelCount(){ return LED_TOTAL_PIXEL_COUNT; }
uint32_t ledDirectOutputLastShowMicros() { return ledLastShowMicros; }
float ledDirectOutputAverageShowMs()     { return ledAvgShowMs; }
float ledDirectOutputFps()               { return ledFps; }
LedOutputMode ledDirectOutputMode()      { return ledDirectRuntimeMode; }
int ledDirectOutputValidationChannel()   { return ledDirectValidationLane; }
LedValidationColor ledDirectOutputValidationColor() { return ledDirectValidationColor; }

uint32_t ledDirectOutputExpectedTransmitMicros() {
  // one WS2812 bit = 1.25 us, 24 bits per pixel, lanes run in parallel so the
  // longest lane sets the frame time
  return (uint32_t)(LED_LONGEST_LANE * 24 * 1.25f);
}

uint8_t ledDirectOutputLanesOnLcd() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < LED_LANE_COUNT; i++) {
    if (strcmp(ledLaneDriver[i], "LCD_CLOCKLESS") == 0) n++;
  }
  return n;
}

bool ledDirectOutputAllLanesOnLcd() {
  return ledDirectOutputLanesOnLcd() == LED_LANE_COUNT;
}

int ledDirectOutputLanePin(uint8_t laneId) {
  const LedLaneConfig *lane = ledLaneFor(laneId);
  return lane ? lane->dataPin : -1;
}

uint16_t ledDirectOutputLanePixelCount(uint8_t laneId) {
  const LedLaneConfig *lane = ledLaneFor(laneId);
  return lane ? lane->pixels : 0;
}

uint16_t ledDirectOutputLaneLogicalStart(uint8_t laneId) {
  const LedLaneConfig *lane = ledLaneFor(laneId);
  return lane ? lane->startIndex : 0;
}

const char *ledDirectOutputLaneDriverName(uint8_t laneId) {
  if (laneId >= LED_LANE_COUNT) return "-";
  return ledLaneDriver[laneId];
}

const char *ledDirectOutputModeName() {
  switch (ledDirectRuntimeMode) {
    case LED_OUTPUT_VALIDATE_COLOR:   return "VALIDATE_COLOR";
    case LED_OUTPUT_VALIDATE_SOLID:   return "VALIDATE_SOLID";
    case LED_OUTPUT_VALIDATE_CHANNEL: return "VALIDATE_CHANNEL";
    case LED_OUTPUT_ANIMATION:        return "ANIMATION";
    case LED_OUTPUT_OFF:
    default:                          return "OFF";
  }
}

const char *ledDirectOutputValidationColorName() {
  switch (ledDirectValidationColor) {
    case LED_VALIDATION_COLOR_GREEN: return "green";
    case LED_VALIDATION_COLOR_BLUE:  return "blue";
    case LED_VALIDATION_COLOR_RED:
    default:                         return "red";
  }
}

// =============================================================================
// MODES
// =============================================================================

bool ledDirectOutputSetMode(LedOutputMode mode, Stream &out) {
  if (mode == LED_OUTPUT_VALIDATE_CHANNEL) {
    out.println("Use: led ch 0..7");
    return false;
  }
  if (mode == LED_OUTPUT_VALIDATE_COLOR) {
    out.println("Use: led red, led green, or led blue");
    return false;
  }

  ledDirectRuntimeMode = mode;
  ledDirectValidationLane = -1;
  out.print("LED mode: ");
  out.println(ledDirectOutputModeName());
  return true;
}

bool ledDirectOutputSetChannelValidationMode(uint8_t laneId, Stream &out) {
  if (laneId >= LED_LANE_COUNT) {
    out.println("LED lane must be 0..7");
    return false;
  }
  ledDirectRuntimeMode = LED_OUTPUT_VALIDATE_CHANNEL;
  ledDirectValidationLane = laneId;
  const LedLaneConfig *lane = ledLaneFor(laneId);
  out.printf("LED mode: VALIDATE_CHANNEL %u  (%s, GPIO%d, %u px)\n",
             laneId, lane->name, lane->dataPin, lane->pixels);
  return true;
}

bool ledDirectOutputSetColorValidationMode(LedValidationColor color, Stream &out) {
  ledDirectRuntimeMode = LED_OUTPUT_VALIDATE_COLOR;
  ledDirectValidationLane = -1;
  ledDirectValidationColor = color;
  out.print("LED mode: VALIDATE_COLOR ");
  out.println(ledDirectOutputValidationColorName());
  return true;
}

// =============================================================================
// DIAGNOSTICS
// =============================================================================

void ledDirectOutputPrintDriverTable(Stream &out) {
  uint8_t onLcd = ledDirectOutputLanesOnLcd();

  out.println("lane  zone           gpio  leds  start  driver");
  for (uint8_t i = 0; i < LED_LANE_COUNT; i++) {
    const LedLaneConfig &lane = LED_LANES[i];
    out.printf("  %u   %-13s %4d  %4u  %5u  %s\n",
               lane.laneId, lane.name, lane.dataPin,
               lane.pixels, lane.startIndex, ledLaneDriver[i]);
  }

  float expectedMs = ledDirectOutputExpectedTransmitMicros() / 1000.0f;
  out.printf("driver : %u/%u on LCD_CLOCKLESS  ->  %s\n",
             onLcd, LED_LANE_COUNT,
             (onLcd == LED_LANE_COUNT) ? "PASS, all lanes parallel"
                                       : "FAIL, see driver column");
  out.printf("pixels : %u logical, %u sent (lanes pad to longest, %u)\n",
             LED_TOTAL_PIXEL_COUNT, LED_LONGEST_LANE * LED_LANE_COUNT,
             LED_LONGEST_LANE);
  out.printf("show   : %.1f ms  =  ~%.1f ms on the wire + ~%.1f ms buffer prep\n",
             ledAvgShowMs, expectedMs, ledAvgShowMs - expectedMs);
  out.printf("rate   : %.1f fps   mode %s\n", ledFps, ledDirectOutputModeName());
}

void ledDirectOutputPrintRuntimeStatus(Stream &out) {
  out.print("LED mode=");
  out.print(ledDirectOutputModeName());
  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_CHANNEL) {
    out.print(" lane=");
    out.print(ledDirectValidationLane);
  }
  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_COLOR) {
    out.print(" color=");
    out.print(ledDirectOutputValidationColorName());
  }
  out.printf(" lanes=%u/%u on LCD  show=%.1fms  fps=%.1f\n",
             ledDirectOutputLanesOnLcd(), LED_LANE_COUNT, ledAvgShowMs, ledFps);
}

bool ledDirectOutputRenderPixelForTest(
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  LedRgbColor &outColor
) {
  if (logicalPixelIndex >= LED_TOTAL_PIXEL_COUNT) {
    outColor = { 0, 0, 0 };
    return false;
  }
  outColor = ledRenderRgb(logicalPixelIndex, nowMs);
  return true;
}
