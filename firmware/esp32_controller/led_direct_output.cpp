#include "led_direct_output.h"

#include <FastLED.h>
#include <esp_heap_caps.h>
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"

#include "led_config.h"
#include "led_color_convert.h"
#include "led_engine.h"
#include "led_settings.h"

constexpr uint8_t FASTLED_LANE_COUNT = 7;
constexpr uint32_t LED_STARTUP_HARDWARE_TEST_MS = 5000;
constexpr bool ENABLE_REAL_FASTLED_OUTPUT = true;
constexpr LedOutputMode DEFAULT_LED_OUTPUT_MODE = LED_OUTPUT_ANIMATION;

struct LedDirectLaneConfig {
  uint8_t laneId;
  uint8_t dataPin;
  uint16_t pixelCount;
  uint16_t startIndex;
};

constexpr LedDirectLaneConfig FASTLED_LANES[FASTLED_LANE_COUNT] = {
  { 1, 1, 208, 0 },     // Z1 mouth
  { 2, 2, 325, 208 },   // Z2 shoulder
  { 3, 39, 400, 533 },  // Z3 midbody
  { 4, 40, 300, 933 },  // Z4 rear
  { 5, 41, 300, 1233 }, // Z5 front legs
  { 6, 42, 300, 1533 }, // Z6 back legs
  { 7, 43, 75, 1833 }   // Z7 digestive
};

static_assert(LED_TOTAL_PIXEL_COUNT == 1908, "Unexpected logical LED count");
static_assert(
  FASTLED_LANES[6].startIndex + FASTLED_LANES[6].pixelCount == LED_TOTAL_PIXEL_COUNT,
  "FastLED frame layout is inconsistent"
);

static CRGB ledFastLedFrame[LED_TOTAL_PIXEL_COUNT];

static bool ledDirectOutputInitialized = false;
static bool ledFastLedOutputArmed = false;
static LedOutputMode ledDirectRuntimeMode = DEFAULT_LED_OUTPUT_MODE;
static int ledDirectValidationLane = -1;
static LedValidationColor ledDirectValidationColor = LED_VALIDATION_COLOR_RED;
static bool ledFastLedFirstShowPending = true;
static bool ledFastLedFirstShowAttempted = false;
static uint8_t ledFastLedFirstShowEnqueuedCount = 0;
static bool ledFastLedFirstShowDriverMismatch = false;

struct LedFastLedHeapSnapshot {
  size_t internal8Bit;
  size_t internalDma;
  size_t psram8Bit;
  size_t largestInternal8Bit;
  size_t largestInternalDma;
  size_t largestPsram8Bit;
};

static const LedDirectLaneConfig *ledDirectLaneConfigFor(uint8_t laneId) {
  for (uint8_t i = 0; i < FASTLED_LANE_COUNT; i++) {
    if (FASTLED_LANES[i].laneId == laneId) {
      return &FASTLED_LANES[i];
    }
  }

  return nullptr;
}

static const LedDirectLaneConfig *ledDirectLaneConfigForLogicalIndex(uint16_t logicalPixelIndex) {
  for (uint8_t i = 0; i < FASTLED_LANE_COUNT; i++) {
    const LedDirectLaneConfig &config = FASTLED_LANES[i];
    if (
      logicalPixelIndex >= config.startIndex
      && logicalPixelIndex < static_cast<uint16_t>(config.startIndex + config.pixelCount)
    ) {
      return &config;
    }
  }

  return nullptr;
}

static LedRgbColor ledDirectValidationColorForLane(uint8_t laneId) {
  static const LedRgbColor colors[8] = {
    { 12, 6, 0 },
    { 8, 8, 8 },
    { 0, 12, 12 },
    { 0, 0, 14 },
    { 10, 0, 14 },
    { 14, 6, 0 },
    { 14, 10, 0 },
    { 14, 0, 0 }
  };

  if (laneId >= 8) {
    return { 0, 0, 0 };
  }

  return colors[laneId];
}

static LedRgbColor ledDirectValidationRgbForColor(LedValidationColor color) {
  switch (color) {
    case LED_VALIDATION_COLOR_GREEN:
      return { 0, 12, 0 };
    case LED_VALIDATION_COLOR_BLUE:
      return { 0, 0, 12 };
    case LED_VALIDATION_COLOR_RED:
    default:
      return { 12, 0, 0 };
  }
}

static LedRgbColor ledDirectRenderRgb(uint16_t logicalPixelIndex, uint32_t nowMs) {
  if (ledDirectRuntimeMode == LED_OUTPUT_OFF) {
    return { 0, 0, 0 };
  }

  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_SOLID) {
    return { 8, 8, 8 };
  }

  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_CHANNEL) {
    const LedDirectLaneConfig *config = ledDirectLaneConfigForLogicalIndex(logicalPixelIndex);
    if (config == nullptr || config->laneId != ledDirectValidationLane) {
      return { 0, 0, 0 };
    }

    return ledDirectValidationColorForLane(config->laneId);
  }

  if (ledDirectRuntimeMode == LED_OUTPUT_VALIDATE_COLOR) {
    return ledDirectValidationRgbForColor(ledDirectValidationColor);
  }

  LedColor hsvColor = ledEngineRenderPixel(logicalPixelIndex, nowMs);
  return ledColorToRgb(hsvColor);
}

static LedFastLedHeapSnapshot ledFastLedCaptureHeap() {
  LedFastLedHeapSnapshot snapshot = {
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
    heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
    heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
  };

  return snapshot;
}

static void ledFastLedPrintHeap(const char *phase, const LedFastLedHeapSnapshot &snapshot) {
  Serial.print("FASTLED FIRST SHOW heap ");
  Serial.print(phase);
  Serial.print(" internal8_free=");
  Serial.print(snapshot.internal8Bit);
  Serial.print(" internal8_largest=");
  Serial.print(snapshot.largestInternal8Bit);
  Serial.print(" internal_dma_free=");
  Serial.print(snapshot.internalDma);
  Serial.print(" internal_dma_largest=");
  Serial.print(snapshot.largestInternalDma);
  Serial.print(" psram8_free=");
  Serial.print(snapshot.psram8Bit);
  Serial.print(" psram8_largest=");
  Serial.println(snapshot.largestPsram8Bit);
}

static void ledFastLedOnChannelEnqueued(const fl::IChannel &channel, const fl::string &driverName) {
  if (!ledFastLedFirstShowPending) {
    return;
  }

  uint8_t laneIndex = ledFastLedFirstShowEnqueuedCount;
  Serial.print("FASTLED FIRST SHOW channel id=");
  Serial.print(channel.id());
  Serial.print(" name=");
  Serial.print(channel.name().c_str());

  if (laneIndex < FASTLED_LANE_COUNT) {
    const LedDirectLaneConfig &config = FASTLED_LANES[laneIndex];
    Serial.print(" zone=Z");
    Serial.print(config.laneId);
    Serial.print(" pin=GPIO");
    Serial.print(config.dataPin);
    Serial.print(" role=REAL");
  } else {
    Serial.print(" role=UNEXPECTED");
  }

  Serial.print(" driver=");
  Serial.println(driverName.c_str());

  if (driverName != "LCD_CLOCKLESS") {
    ledFastLedFirstShowDriverMismatch = true;
  }

  ledFastLedFirstShowEnqueuedCount++;
}

static void ledFastLedRegisterControllers() {
  FastLED.channelEvents().onChannelEnqueued.add(ledFastLedOnChannelEnqueued);
  fl::enableDrivers<fl::Bus::LCD_CLOCKLESS>();

  const auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
  fl::ChannelOptions options;
  options.mBus = fl::Bus::LCD_CLOCKLESS;

  for (uint8_t laneIndex = 0; laneIndex < FASTLED_LANE_COUNT; laneIndex++) {
    const LedDirectLaneConfig &lane = FASTLED_LANES[laneIndex];
    fl::ChannelConfig config(
      fl::ClocklessChipset(lane.dataPin, timing),
      fl::span<CRGB>(ledFastLedFrame + lane.startIndex, lane.pixelCount),
      GRB,
      options
    );
    FastLED.add(fl::Channel::create(config));
  }

  FastLED.setBrightness(255);
}

static void ledDirectOutputStart() {
  if (ledFastLedOutputArmed) {
    return;
  }

  ledFastLedOutputArmed = true;
}

void ledDirectOutputBegin() {
  fill_solid(ledFastLedFrame, LED_TOTAL_PIXEL_COUNT, CRGB::Black);
  ledFastLedRegisterControllers();

  ledDirectOutputInitialized = true;
  Serial.println("FastLED direct backend: READY");
  Serial.println("FastLED lanes: GPIO1,2,39,40,41,42,43; GPIO0 internal LCD_CLOCKLESS dummy/padding, unwired");
}

static bool ledDirectOutputStartIfAllowed(Stream &out) {
  if (!ENABLE_REAL_FASTLED_OUTPUT) {
    out.println("LED real output blocked: ENABLE_REAL_FASTLED_OUTPUT=false");
    return false;
  }

  ledDirectOutputStart();
  return true;
}

static void ledDirectOutputShowFrameIfStarted(uint32_t nowMs) {
  if (!ENABLE_REAL_FASTLED_OUTPUT || !ledFastLedOutputArmed) {
    return;
  }

  for (uint16_t logicalPixelIndex = 0; logicalPixelIndex < LED_TOTAL_PIXEL_COUNT; logicalPixelIndex++) {
    LedRgbColor rgb = ledDirectRenderRgb(logicalPixelIndex, nowMs);
    ledFastLedFrame[logicalPixelIndex] = CRGB(rgb.r, rgb.g, rgb.b);
  }

  if (ledFastLedFirstShowPending) {
    ledFastLedFirstShowEnqueuedCount = 0;
    ledFastLedFirstShowDriverMismatch = false;
    Serial.println("FASTLED FIRST SHOW begin");
    Serial.println("FASTLED FIRST SHOW dummy pin=GPIO0 role=INTERNAL_UNWIRED driver=LCD_CLOCKLESS no_animation_data=1");
    LedFastLedHeapSnapshot before = ledFastLedCaptureHeap();
    ledFastLedPrintHeap("before", before);

    ledFastLedFirstShowAttempted = true;
    FastLED.show();

    LedFastLedHeapSnapshot after = ledFastLedCaptureHeap();
    ledFastLedPrintHeap("after", after);
    if (ledFastLedFirstShowEnqueuedCount == FASTLED_LANE_COUNT && !ledFastLedFirstShowDriverMismatch) {
      Serial.println("ROUTING CONFIRMED: 7/7 real lanes use LCD_CLOCKLESS");
    } else {
      Serial.print("ROUTING FAILED: enqueued=");
      Serial.print(ledFastLedFirstShowEnqueuedCount);
      Serial.print(" expected=7 driverMismatch=");
      Serial.println(ledFastLedFirstShowDriverMismatch ? 1 : 0);
    }
    ledFastLedFirstShowPending = false;
    return;
  }

  FastLED.show();
}

void ledDirectOutputUpdate(uint32_t nowMs) {
  if (ledDirectRuntimeMode == LED_OUTPUT_OFF || !ENABLE_REAL_FASTLED_OUTPUT) {
    return;
  }

  if (!ledFastLedOutputArmed) {
    ledDirectOutputStart();
  }

  ledDirectOutputShowFrameIfStarted(nowMs);
}

void ledDirectOutputRunStartupHardwareTest(Stream &out) {
  if (!ENABLE_REAL_FASTLED_OUTPUT || !ledDirectOutputInitialized) {
    out.println("LED HARDWARE TEST ERROR: FastLED output is unavailable");
    return;
  }

  ledEngineSetStartupHardwareTestEnabled(true);
  ledDirectOutputStart();

  out.println("LED HARDWARE TEST: 5 seconds, moving animation, brightness 4-15%, speed 100%");

  uint32_t firstFrameNowMs = millis();
  ledEngineUpdate(firstFrameNowMs);
  ledDirectOutputShowFrameIfStarted(firstFrameNowMs);
  uint32_t testStartMs = millis();

  while ((millis() - testStartMs) < LED_STARTUP_HARDWARE_TEST_MS) {
    uint32_t nowMs = millis();
    ledEngineUpdate(nowMs);
    ledDirectOutputShowFrameIfStarted(nowMs);
    delay(1);
  }

  ledEngineSetStartupHardwareTestEnabled(false);

  uint32_t normalNowMs = millis();
  ledEngineUpdate(normalNowMs);
  ledDirectOutputShowFrameIfStarted(normalNowMs);

  out.println("LED HARDWARE TEST COMPLETE: saved settings restored");
  if (ledSettingsAmbientIsCompletelyDark()) {
    out.println("WARNING: saved ambient settings are completely dark");
  }
}

bool ledDirectOutputAllowed() {
  return ENABLE_REAL_FASTLED_OUTPUT;
}

bool ledDirectOutputFirstShowAttempted() {
  return ledFastLedFirstShowAttempted;
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

bool ledDirectOutputSetMode(LedOutputMode mode, Stream &out) {
  if (mode == LED_OUTPUT_VALIDATE_CHANNEL) {
    out.println("Use: led ch 1..7");
    return false;
  }

  if (mode == LED_OUTPUT_VALIDATE_COLOR) {
    out.println("Use: led red, led green, or led blue");
    return false;
  }

  if (mode == LED_OUTPUT_OFF) {
    ledDirectRuntimeMode = LED_OUTPUT_OFF;
    ledDirectValidationLane = -1;
    ledDirectValidationColor = LED_VALIDATION_COLOR_RED;

    if (ledFastLedFirstShowAttempted) {
      ledDirectOutputShowFrameIfStarted(millis());
    }

    out.println("LED mode: OFF");
    return true;
  }

  if (!ledDirectOutputStartIfAllowed(out)) {
    return false;
  }

  ledDirectRuntimeMode = mode;
  ledDirectValidationLane = -1;
  out.print("LED mode: ");
  out.println(ledDirectOutputModeName());
  return true;
}

bool ledDirectOutputSetLaneValidationMode(uint8_t laneId, Stream &out) {
  if (ledDirectLaneConfigFor(laneId) == nullptr) {
    out.println("LED channel must be 1..7");
    return false;
  }

  if (!ledDirectOutputStartIfAllowed(out)) {
    return false;
  }

  ledDirectRuntimeMode = LED_OUTPUT_VALIDATE_CHANNEL;
  ledDirectValidationLane = laneId;
  ledDirectValidationColor = LED_VALIDATION_COLOR_RED;
  out.print("LED mode: VALIDATE_CHANNEL ");
  out.println(laneId);
  return true;
}

bool ledDirectOutputSetColorValidationMode(LedValidationColor color, Stream &out) {
  if (!ledDirectOutputStartIfAllowed(out)) {
    return false;
  }

  ledDirectRuntimeMode = LED_OUTPUT_VALIDATE_COLOR;
  ledDirectValidationLane = -1;
  ledDirectValidationColor = color;
  out.print("LED mode: VALIDATE_COLOR ");
  out.println(ledDirectOutputValidationColorName());
  return true;
}

void ledDirectOutputPrintRuntimeStatus(Stream &out) {
  out.print("LED backend=FastLED/LCD_CLOCKLESS mode=");
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
  out.print(" firstShowAttempted=");
  out.print(ledDirectOutputFirstShowAttempted() ? 1 : 0);
  out.print(" savedDark=");
  out.print(ledSettingsAmbientIsCompletelyDark() ? 1 : 0);
  out.print(" lanes=7 pixels=");
  out.print(LED_TOTAL_PIXEL_COUNT);
  out.println(" z3=400 pins=1,2,39,40,41,42,43 dummy=0 order=GRB");
}
