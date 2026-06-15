#include "led_expander_output.h"

#include <functional>
#include <memory>
#include <vector>

#include "led_config.h"
#include "led_engine.h"
#include "src/PBDriverAdapter/src/PBDriverAdapter.hpp"

constexpr int PB_EXPANDER_TX_PIN = 39;
constexpr uint32_t PB_EXPANDER_BAUD_RATE = 2000000;
constexpr uint8_t PB_EXPANDER_CHANNEL_COUNT = 8;
constexpr uint32_t PB_EXPANDER_WS2812_FREQUENCY = 800000;

// California hardware validation only. Keep false for India/no-expander testing.
constexpr bool ENABLE_REAL_PB_EXPANDER_OUTPUT = false;

struct LedExpanderChannelConfig {
  uint8_t channelId;
  uint16_t pixels;
  uint16_t startIndex;
};

const LedExpanderChannelConfig PB_EXPANDER_CHANNELS[PB_EXPANDER_CHANNEL_COUNT] = {
  { 0, 100, 1908 }, // Ch0 = Z8
  { 1, 208, 0 },    // Ch1 = Z1
  { 2, 325, 208 },  // Ch2 = Z2
  { 3, 400, 533 },  // Ch3 = Z3
  { 4, 300, 933 },  // Ch4 = Z4
  { 5, 300, 1233 }, // Ch5 = Z5
  { 6, 300, 1533 }, // Ch6 = Z6
  { 7, 75, 1833 }   // Ch7 = Z7
};

static const LedExpanderChannelConfig *ledExpanderChannelConfigFor(uint8_t channelId) {
  for (uint8_t i = 0; i < PB_EXPANDER_CHANNEL_COUNT; i++) {
    if (PB_EXPANDER_CHANNELS[i].channelId == channelId) {
      return &PB_EXPANDER_CHANNELS[i];
    }
  }

  return nullptr;
}

static void ledExpanderPackRgbAsGrb(const LedRgbColor &rgb, uint8_t *outBytes, uint8_t outCapacity) {
  outBytes[0] = rgb.g;
  outBytes[1] = rgb.r;
  outBytes[2] = rgb.b;

  if (outCapacity >= 4) {
    outBytes[3] = 0;
  }
}

static uint32_t ledExpanderChecksumByte(uint32_t checksum, uint8_t value) {
  return checksum * 131u + value;
}

static void ledExpanderPrintByteHex(Stream &out, uint8_t value) {
  if (value < 16) {
    out.print('0');
  }

  out.print(value, HEX);
}

static void ledExpanderPrintPackedPixel(Stream &out, const uint8_t pixel[3]) {
  ledExpanderPrintByteHex(out, pixel[0]);
  out.print(',');
  ledExpanderPrintByteHex(out, pixel[1]);
  out.print(',');
  ledExpanderPrintByteHex(out, pixel[2]);
}

static void ledExpanderClearFrameStats(LedExpanderFrameStats &stats) {
  stats.channelCount = 0;
  stats.totalPixels = 0;
  stats.failedPixels = 0;
  stats.frameChecksum = 0;

  for (uint8_t channelIndex = 0; channelIndex < 8; channelIndex++) {
    LedExpanderChannelFrameStats &channelStats = stats.channels[channelIndex];
    channelStats.channelId = 0;
    channelStats.logicalStartIndex = 0;
    channelStats.pixelCount = 0;
    channelStats.checksum = 0;

    for (uint8_t byteIndex = 0; byteIndex < 3; byteIndex++) {
      channelStats.firstPixel[byteIndex] = 0;
      channelStats.lastPixel[byteIndex] = 0;
    }
  }
}

static PBDriverAdapter ledExpanderDriver;
static bool ledExpanderOutputEnabled = false;
static bool ledExpanderOutputInitialized = false;
static bool ledExpanderRealOutputStarted = false;
static LedOutputMode ledExpanderRuntimeMode = LED_OUTPUT_OFF;
static int ledExpanderValidationChannel = -1;
static uint8_t ledExpanderConfiguredChannelCount = 0;
static uint16_t ledExpanderConfiguredPixelCount = 0;
static uint32_t ledExpanderRenderNowMs = 0;

static const LedExpanderChannelConfig *ledExpanderChannelConfigForLogicalIndex(uint16_t logicalPixelIndex) {
  for (uint8_t i = 0; i < PB_EXPANDER_CHANNEL_COUNT; i++) {
    const LedExpanderChannelConfig &config = PB_EXPANDER_CHANNELS[i];
    if (
      logicalPixelIndex >= config.startIndex
      && logicalPixelIndex < static_cast<uint16_t>(config.startIndex + config.pixels)
    ) {
      return &config;
    }
  }

  return nullptr;
}

static LedRgbColor ledExpanderValidationColorForChannel(uint8_t channelId) {
  static const LedRgbColor colors[PB_EXPANDER_CHANNEL_COUNT] = {
    { 12, 6, 0 },
    { 8, 8, 8 },
    { 0, 12, 12 },
    { 0, 0, 14 },
    { 10, 0, 14 },
    { 14, 6, 0 },
    { 14, 10, 0 },
    { 14, 0, 0 }
  };

  if (channelId >= PB_EXPANDER_CHANNEL_COUNT) {
    return { 0, 0, 0 };
  }

  return colors[channelId];
}

static void ledExpanderPackBlack(uint8_t rgbw[]) {
  rgbw[0] = 0;
  rgbw[1] = 0;
  rgbw[2] = 0;
  rgbw[3] = 0;
}

static void ledExpanderRenderCallback(uint16_t logicalPixelIndex, uint8_t rgbw[]) {
  if (ledExpanderRuntimeMode == LED_OUTPUT_OFF) {
    ledExpanderPackBlack(rgbw);
    return;
  }

  if (ledExpanderRuntimeMode == LED_OUTPUT_VALIDATE_SOLID) {
    LedRgbColor rgb = { 8, 8, 8 };
    ledExpanderPackRgbAsGrb(rgb, rgbw, 4);
    return;
  }

  if (ledExpanderRuntimeMode == LED_OUTPUT_VALIDATE_CHANNEL) {
    const LedExpanderChannelConfig *config = ledExpanderChannelConfigForLogicalIndex(logicalPixelIndex);
    if (config == nullptr || config->channelId != ledExpanderValidationChannel) {
      ledExpanderPackBlack(rgbw);
      return;
    }

    LedRgbColor rgb = ledExpanderValidationColorForChannel(config->channelId);
    ledExpanderPackRgbAsGrb(rgb, rgbw, 4);
    return;
  }

  if (!ledExpanderOutputRenderPackedPixelForTest(logicalPixelIndex, ledExpanderRenderNowMs, rgbw, 4)) {
    ledExpanderPackBlack(rgbw);
  }
}

static void ledExpanderChannelSwitchCallback(PBChannel *channel) {
  (void)channel;
}

void ledExpanderOutputBegin() {
  std::unique_ptr<std::vector<PBChannel>> channels(new std::vector<PBChannel>(PB_EXPANDER_CHANNEL_COUNT));
  uint16_t configuredPixels = 0;

  for (uint8_t i = 0; i < PB_EXPANDER_CHANNEL_COUNT; i++) {
    PBChannel &channel = (*channels)[i];
    const LedExpanderChannelConfig &config = PB_EXPANDER_CHANNELS[i];

    channel.channelId = config.channelId;
    channel.channelType = CHANNEL_WS2812;
    channel.numElements = 3;
    channel.redi = 1;
    channel.greeni = 0;
    channel.bluei = 2;
    channel.whitei = 0;
    channel.pixels = config.pixels;
    channel.startIndex = config.startIndex;
    channel.frequency = PB_EXPANDER_WS2812_FREQUENCY;

    configuredPixels += config.pixels;
  }

  ledExpanderDriver.configureChannels(std::move(channels));
  ledExpanderConfiguredChannelCount = PB_EXPANDER_CHANNEL_COUNT;
  ledExpanderConfiguredPixelCount = configuredPixels;
  ledExpanderOutputInitialized = true;
}

static bool ledExpanderOutputStartRealOutputIfAllowed(Stream &out) {
  if (!ENABLE_REAL_PB_EXPANDER_OUTPUT) {
    out.println("LED real output blocked: ENABLE_REAL_PB_EXPANDER_OUTPUT=false");
    return false;
  }

  if (!ledExpanderRealOutputStarted) {
    ledExpanderDriver.begin(PB_EXPANDER_BAUD_RATE, PB_EXPANDER_TX_PIN);
    ledExpanderRealOutputStarted = true;
  }

  return true;
}

static void ledExpanderOutputShowFrameIfStarted(uint32_t nowMs) {
  if (!ENABLE_REAL_PB_EXPANDER_OUTPUT || !ledExpanderRealOutputStarted) {
    return;
  }

  ledExpanderRenderNowMs = nowMs;
  ledExpanderDriver.show(
    LED_TOTAL_PIXEL_COUNT,
    ledExpanderRenderCallback,
    ledExpanderChannelSwitchCallback
  );
}

void ledExpanderOutputUpdate(uint32_t nowMs) {
  if (ledExpanderRuntimeMode == LED_OUTPUT_OFF) {
    return;
  }

  if (!ENABLE_REAL_PB_EXPANDER_OUTPUT) {
    return;
  }

  if (!ledExpanderRealOutputStarted) {
    return;
  }

  ledExpanderOutputShowFrameIfStarted(nowMs);
}

bool ledExpanderOutputIsEnabled() {
  return ledExpanderOutputEnabled;
}

void ledExpanderOutputSetEnabled(bool enabled) {
  ledExpanderOutputEnabled = enabled;
}

bool ledExpanderOutputIsInitialized() {
  return ledExpanderOutputInitialized;
}

bool ledExpanderOutputRealOutputAllowed() {
  return ENABLE_REAL_PB_EXPANDER_OUTPUT;
}

bool ledExpanderOutputRealOutputStarted() {
  return ledExpanderRealOutputStarted;
}

uint8_t ledExpanderOutputConfiguredChannelCount() {
  return ledExpanderConfiguredChannelCount;
}

uint16_t ledExpanderOutputConfiguredPixelCount() {
  return ledExpanderConfiguredPixelCount;
}

uint32_t ledExpanderOutputPlannedBaudRate() {
  return PB_EXPANDER_BAUD_RATE;
}

int ledExpanderOutputPlannedTxPin() {
  return PB_EXPANDER_TX_PIN;
}

LedOutputMode ledExpanderOutputMode() {
  return ledExpanderRuntimeMode;
}

int ledExpanderOutputValidationChannel() {
  return ledExpanderValidationChannel;
}

const char *ledExpanderOutputModeName() {
  switch (ledExpanderRuntimeMode) {
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

bool ledExpanderOutputSetMode(LedOutputMode mode, Stream &out) {
  if (mode == LED_OUTPUT_VALIDATE_CHANNEL) {
    out.println("Use: led ch 0..7");
    return false;
  }

  if (mode == LED_OUTPUT_OFF) {
    ledExpanderRuntimeMode = LED_OUTPUT_OFF;
    ledExpanderValidationChannel = -1;

    if (ENABLE_REAL_PB_EXPANDER_OUTPUT && ledExpanderRealOutputStarted) {
      ledExpanderOutputShowFrameIfStarted(millis());
    }

    out.println("LED mode: OFF");
    return true;
  }

  if (!ledExpanderOutputStartRealOutputIfAllowed(out)) {
    return false;
  }

  ledExpanderRuntimeMode = mode;
  ledExpanderValidationChannel = -1;
  out.print("LED mode: ");
  out.println(ledExpanderOutputModeName());
  return true;
}

bool ledExpanderOutputSetChannelValidationMode(uint8_t channelId, Stream &out) {
  if (channelId >= PB_EXPANDER_CHANNEL_COUNT) {
    out.println("LED channel must be 0..7");
    return false;
  }

  if (!ledExpanderOutputStartRealOutputIfAllowed(out)) {
    return false;
  }

  ledExpanderRuntimeMode = LED_OUTPUT_VALIDATE_CHANNEL;
  ledExpanderValidationChannel = channelId;
  out.print("LED mode: VALIDATE_CHANNEL ");
  out.println(channelId);
  return true;
}

void ledExpanderOutputPrintRuntimeStatus(Stream &out) {
  out.print("LED mode=");
  out.print(ledExpanderOutputModeName());

  if (ledExpanderRuntimeMode == LED_OUTPUT_VALIDATE_CHANNEL) {
    out.print(" ch=");
    out.print(ledExpanderValidationChannel);
  }

  out.print(" allowed=");
  out.print(ledExpanderOutputRealOutputAllowed() ? 1 : 0);
  out.print(" started=");
  out.print(ledExpanderOutputRealOutputStarted() ? 1 : 0);
  out.print(" tx=");
  out.print(ledExpanderOutputPlannedTxPin());
  out.print(" baud=");
  out.println(ledExpanderOutputPlannedBaudRate());
}

uint16_t ledExpanderOutputChannelPixelCount(uint8_t channelId) {
  const LedExpanderChannelConfig *config = ledExpanderChannelConfigFor(channelId);
  if (config == nullptr) {
    return 0;
  }

  return config->pixels;
}

uint16_t ledExpanderOutputChannelLogicalStart(uint8_t channelId) {
  const LedExpanderChannelConfig *config = ledExpanderChannelConfigFor(channelId);
  if (config == nullptr) {
    return 0;
  }

  return config->startIndex;
}

bool ledExpanderOutputRenderPixelForTest(
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  LedRgbColor &outColor
) {
  if (logicalPixelIndex >= LED_TOTAL_PIXEL_COUNT) {
    outColor = { 0, 0, 0 };
    return false;
  }

  LedColor hsvColor = ledEngineRenderPixel(logicalPixelIndex, nowMs);
  outColor = ledColorToRgb(hsvColor);
  return true;
}

bool ledExpanderOutputRenderChannelForTest(
  uint8_t channelId,
  uint32_t nowMs,
  LedRgbColor *outPixels,
  uint16_t outCapacity,
  uint16_t &outPixelCount
) {
  outPixelCount = 0;

  const LedExpanderChannelConfig *config = ledExpanderChannelConfigFor(channelId);
  if (config == nullptr || outPixels == nullptr || outCapacity < config->pixels) {
    return false;
  }

  for (uint16_t localIndex = 0; localIndex < config->pixels; localIndex++) {
    uint16_t logicalPixelIndex = config->startIndex + localIndex;
    if (!ledExpanderOutputRenderPixelForTest(logicalPixelIndex, nowMs, outPixels[localIndex])) {
      outPixelCount = 0;
      return false;
    }
  }

  outPixelCount = config->pixels;
  return true;
}

bool ledExpanderOutputRenderPackedPixelForTest(
  uint16_t logicalPixelIndex,
  uint32_t nowMs,
  uint8_t *outBytes,
  uint8_t outCapacity
) {
  if (outBytes == nullptr || outCapacity < 3) {
    return false;
  }

  LedRgbColor rgb = { 0, 0, 0 };
  bool rendered = ledExpanderOutputRenderPixelForTest(logicalPixelIndex, nowMs, rgb);
  ledExpanderPackRgbAsGrb(rgb, outBytes, outCapacity);
  return rendered;
}

bool ledExpanderOutputPrepareCallbacksForTest(uint32_t nowMs) {
  ledExpanderRenderNowMs = nowMs;

  std::function<void(uint16_t, uint8_t[])> renderFn = ledExpanderRenderCallback;
  std::function<void(PBChannel *)> channelFn = ledExpanderChannelSwitchCallback;

  (void)renderFn;
  (void)channelFn;
  return true;
}

bool ledExpanderOutputSimulateFrameForTest(
  uint32_t nowMs,
  LedExpanderFrameStats &outStats
) {
  ledExpanderClearFrameStats(outStats);

  uint16_t totalPixels = 0;
  uint16_t failedPixels = 0;
  uint32_t frameChecksum = 0;

  for (uint8_t channelIndex = 0; channelIndex < PB_EXPANDER_CHANNEL_COUNT; channelIndex++) {
    const LedExpanderChannelConfig &config = PB_EXPANDER_CHANNELS[channelIndex];
    LedExpanderChannelFrameStats &channelStats = outStats.channels[channelIndex];

    channelStats.channelId = config.channelId;
    channelStats.logicalStartIndex = config.startIndex;
    channelStats.pixelCount = config.pixels;

    uint32_t channelChecksum = 0;
    channelChecksum = ledExpanderChecksumByte(channelChecksum, config.channelId);
    channelChecksum = ledExpanderChecksumByte(channelChecksum, static_cast<uint8_t>(config.startIndex & 0xff));
    channelChecksum = ledExpanderChecksumByte(channelChecksum, static_cast<uint8_t>(config.startIndex >> 8));
    channelChecksum = ledExpanderChecksumByte(channelChecksum, static_cast<uint8_t>(config.pixels & 0xff));
    channelChecksum = ledExpanderChecksumByte(channelChecksum, static_cast<uint8_t>(config.pixels >> 8));

    for (uint16_t localIndex = 0; localIndex < config.pixels; localIndex++) {
      uint8_t packed[4] = { 0, 0, 0, 0 };
      uint16_t logicalPixelIndex = config.startIndex + localIndex;
      bool rendered = ledExpanderOutputRenderPackedPixelForTest(logicalPixelIndex, nowMs, packed, 4);

      if (!rendered) {
        failedPixels++;
      }

      if (localIndex == 0) {
        channelStats.firstPixel[0] = packed[0];
        channelStats.firstPixel[1] = packed[1];
        channelStats.firstPixel[2] = packed[2];
      }

      channelStats.lastPixel[0] = packed[0];
      channelStats.lastPixel[1] = packed[1];
      channelStats.lastPixel[2] = packed[2];

      channelChecksum = ledExpanderChecksumByte(channelChecksum, static_cast<uint8_t>(localIndex & 0xff));
      channelChecksum = ledExpanderChecksumByte(channelChecksum, static_cast<uint8_t>(localIndex >> 8));
      channelChecksum = ledExpanderChecksumByte(channelChecksum, packed[0]);
      channelChecksum = ledExpanderChecksumByte(channelChecksum, packed[1]);
      channelChecksum = ledExpanderChecksumByte(channelChecksum, packed[2]);
      totalPixels++;
    }

    channelStats.checksum = channelChecksum;
    frameChecksum = ledExpanderChecksumByte(frameChecksum, config.channelId);
    frameChecksum = frameChecksum * 131u + channelChecksum;
  }

  outStats.channelCount = PB_EXPANDER_CHANNEL_COUNT;
  outStats.totalPixels = totalPixels;
  outStats.failedPixels = failedPixels;
  outStats.frameChecksum = frameChecksum;

  return outStats.channelCount == PB_EXPANDER_CHANNEL_COUNT
    && outStats.totalPixels == LED_TOTAL_PIXEL_COUNT
    && outStats.failedPixels == 0;
}

void ledExpanderOutputPrintSimFrameStats(uint32_t nowMs, Stream &out) {
  LedExpanderFrameStats stats;
  bool ok = ledExpanderOutputSimulateFrameForTest(nowMs, stats);

  out.print("EXP REAL allowed=");
  out.print(ledExpanderOutputRealOutputAllowed() ? 1 : 0);
  out.print(" started=");
  out.print(ledExpanderOutputRealOutputStarted() ? 1 : 0);
  out.print(" tx=");
  out.print(ledExpanderOutputPlannedTxPin());
  out.print(" baud=");
  out.println(ledExpanderOutputPlannedBaudRate());

  out.print("EXP SIM ");
  out.print(ok ? "OK" : "FAIL");
  out.print(" channels=");
  out.print(stats.channelCount);
  out.print(" pixels=");
  out.print(stats.totalPixels);
  out.print(" failed=");
  out.print(stats.failedPixels);
  out.print(" checksum=");
  out.print(stats.frameChecksum);
  out.println(" byteOrder=GRB");

  for (uint8_t channelIndex = 0; channelIndex < stats.channelCount; channelIndex++) {
    const LedExpanderChannelFrameStats &channelStats = stats.channels[channelIndex];

    out.print("CH");
    out.print(channelStats.channelId);
    out.print(" start=");
    out.print(channelStats.logicalStartIndex);
    out.print(" px=");
    out.print(channelStats.pixelCount);
    out.print(" first=");
    ledExpanderPrintPackedPixel(out, channelStats.firstPixel);
    out.print(" last=");
    ledExpanderPrintPackedPixel(out, channelStats.lastPixel);
    out.print(" sum=");
    out.println(channelStats.checksum);
  }
}
