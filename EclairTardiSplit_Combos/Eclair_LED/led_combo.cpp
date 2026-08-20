#include "led_combo.h"

#include <math.h>

// =============================================================================
// MOOD TABLE
// =============================================================================
//
// hueShift     added to every zone's own hue at ALL times, including ambient.
//              This is what makes Button 8 visible when nobody is pressing
//              anything. Preserves the RELATIVE colour relationships across the
//              sculpture while rotating the whole wheel.
//
// signature    the colour this mood flashes on toggle, so a B8 press is
//              unmistakable rather than something you have to look for.
//
// lengthScale  multiplies the space-shaped zones' gradient/tail/zap lengths.
//              <1 = shorter, tighter, higher contrast. Also cheaper: fewer
//              consecutive lit pixels is less current.
//
// speedPercent animation speed for the time-shaped zones, as an INTEGER
//              percentage. 100 = unchanged. Integer because the engine folds
//              this into a uint64 time calculation - a float there quantises
//              the time base after a few minutes of uptime.

struct LedMoodConfig {
  const char *name;
  float hueShift;
  float signature;
  float lengthScale;
  uint16_t speedPercent;
};

static const LedMoodConfig LED_MOODS[LED_MOOD_COUNT] = {
  // ORGANIC — MUST BE AN EXACT NO-OP at rest. This is the sculpture as built.
  //
  // An earlier version had lengthScale 1.35 here, on the reasoning that
  // ORGANIC should be the "long, flowing" mood. That was wrong: it stretched
  // Z8's chase tail from 5 px to 6.75 px on a 14 px station string - nearly
  // half the run - turning a crisp chase into a smear, by default, before
  // anyone had pressed anything.
  //
  // The default look is not a design opportunity. Leave these at identity.
  { "ORGANIC", 0.00f, 0.50f, 1.00f, 100 },

  // CHARGED — the only mood that deviates. Warm, quick, tight.
  { "CHARGED", 0.50f, 0.02f, 0.60f, 160 },
};

// =============================================================================
// COLOUR CODE
// =============================================================================
//
// One distinct colour per number of LIT ZONES. Chosen by brute-force search
// over orderings of seven LED-legible hues, maximising the smallest gap
// between consecutive counts.
//
//   minimum gap between adjacent counts : 0.30
//   minimum gap between the two moods   : 0.30
//
// WHY THAT MATTERS. The first table walked green -> cyan -> blue -> violet and
// gave 3 and 4 the same hue, 5 and 6 the same, 7 and 8 the same. Two failures
// at once:
//
//   1. A fourth person could join and NOTHING visibly changed.
//   2. The steps that did change were 0.07-0.08 apart, along one side of the
//      wheel, in the region where hue discrimination is worst. Everything past
//      three buttons looked like the same blue.
//
// That is why four buttons onward read as confusing. Every count is now its
// own clearly separated colour, and every count is a different colour in the
// other mood, so Button 8 always visibly does something.
//
// These are deliberately NOT a temperature ramp. A ramp is prettier in theory
// but HSV compresses red->yellow into 0.00-0.15, so any ramp has tiny gaps at
// the warm end. Legibility at thirty feet in the dark beats narrative.

static const float COMBO_HUE[LED_MOOD_COUNT][9] = {
  //          0      1     2      3      4        5       6      7       8
  /* ORG */ {0.00f, 0.00f, 0.00f, 0.33f, 0.85f,  0.15f,  0.50f, 0.08f,  0.67f},
  //                       red    green  magenta yellow  cyan   orange  blue
  /* CHG */ {0.00f, 0.00f, 0.33f, 0.00f, 0.50f,  0.85f,  0.15f, 0.67f,  0.08f},
  //                       green  red    cyan    magenta yellow blue    orange
};

// Names for the serial banner. Being able to read what colour the code THINKS
// it is showing turns "that looked wrong" into a two-second check.
static const char *COMBO_NAME[LED_MOOD_COUNT][9] = {
  /* ORG */ {"-","-","red","green","magenta","yellow","cyan","orange","blue"},
  /* CHG */ {"-","-","green","red","cyan","magenta","yellow","blue","orange"},
};

// Full commitment at two zones. A partial blend is not readable at distance.
static float ledComboBlendForCount(uint8_t litZones) {
  return (litZones >= 2) ? 1.0f : 0.0f;
}

// =============================================================================
// STATE
// =============================================================================

static LedMood ledCurrentMood = LED_MOOD_ORGANIC;
static uint8_t ledButtonsHeld = 0;
static uint8_t ledHeldMask = 0;      // bit N = zone N is held
static uint8_t ledLitZones = 0;      // popcount(mask) - drives WHICH colour
static float ledBlend = 0.0f;
static float ledBlendSmoothed = 0.0f;

static float ledTargetHue = 0.0f;    // straight from the table
static float ledActiveHue = 0.0f;    // eased toward the target

// Blend: snappy but not jarring. At ~43 fps this reaches 90% in about 150 ms,
// so the colour arrives while the person's finger is still moving.
static const float LED_BLEND_EASE = 0.18f;

// Hue: eases BETWEEN colours rather than snapping.
//
// Without this, a group arriving one at a time made the colour jump on every
// press - red, green, magenta, yellow in under a second - which is a large
// part of why four or more buttons read as chaotic. Sliding round the wheel
// instead makes a growing crowd feel like one continuous change.
static const float LED_HUE_EASE = 0.10f;

// Mood confirmation flash. Toggling B8 briefly forces the whole sculpture to
// the new mood's signature colour, so the press is unmistakable even when
// nobody else is pressing anything.
static uint32_t ledMoodFlashUntilMs = 0;
static const uint32_t LED_MOOD_FLASH_MS = 700;

static uint8_t ledPopCount(uint8_t v) {
  uint8_t n = 0;
  while (v) { n += (v & 1u); v >>= 1; }
  return n;
}

void ledComboBegin() {
  ledCurrentMood = LED_MOOD_ORGANIC;
  ledButtonsHeld = 0;
  ledHeldMask = 0;
  ledLitZones = 0;
  ledBlend = 0.0f;
  ledBlendSmoothed = 0.0f;
  ledTargetHue = 0.0f;
  ledActiveHue = 0.0f;
  ledMoodFlashUntilMs = 0;
}

// count is reported for diagnostics only. The COLOUR is driven by how many
// zones are actually lit, not by how many buttons are down.
//
// Those differ because Button 8 has no zone. Driving the colour off the raw
// button count meant holding B8 alongside three zone buttons showed the
// four-zone colour while only three zones were lit - and releasing B8 changed
// the colour without changing anything you could see. Counting lit zones keeps
// what you see and what you did in agreement.
void ledComboSetButtonsHeld(uint8_t count, uint8_t heldZoneMask) {
  if (count > 8) count = 8;
  ledButtonsHeld = count;
  ledHeldMask = heldZoneMask;

  ledLitZones = ledPopCount(heldZoneMask);
  if (ledLitZones > 8) ledLitZones = 8;

  ledBlend = ledComboBlendForCount(ledLitZones);
  ledBlendSmoothed += (ledBlend - ledBlendSmoothed) * LED_BLEND_EASE;

  // Below two zones the target is left alone deliberately, so the colour fades
  // out through whatever it last was rather than lurching somewhere new on the
  // way down.
  if (ledLitZones >= 2) {
    ledTargetHue = COMBO_HUE[ledCurrentMood][ledLitZones];
  }

  ledActiveHue = ledComboLerpHue(ledActiveHue, ledTargetHue, LED_HUE_EASE);
}

void ledComboToggleMood() {
  ledCurrentMood = (ledCurrentMood == LED_MOOD_ORGANIC)
    ? LED_MOOD_CHARGED
    : LED_MOOD_ORGANIC;
  ledMoodFlashUntilMs = millis() + LED_MOOD_FLASH_MS;

  // Re-pick the colour for the current count immediately. Without this the
  // sculpture kept the OLD mood's colour until somebody changed the number of
  // buttons held, which made the mood toggle look like it had not worked.
  if (ledLitZones >= 2) {
    ledTargetHue = COMBO_HUE[ledCurrentMood][ledLitZones];
  }
}

void ledComboSetMood(LedMood mood) {
  if (mood < LED_MOOD_COUNT) {
    ledCurrentMood = mood;
    if (ledLitZones >= 2) ledTargetHue = COMBO_HUE[ledCurrentMood][ledLitZones];
  }
}

LedMood ledComboMood()          { return ledCurrentMood; }
const char *ledComboMoodName()  { return LED_MOODS[ledCurrentMood].name; }
uint8_t ledComboButtonsHeld()   { return ledButtonsHeld; }
uint8_t ledComboHeldMask()      { return ledHeldMask; }
uint8_t ledComboLitZones()      { return ledLitZones; }
float ledComboBlend()           { return ledBlendSmoothed; }
float ledComboLengthScale()     { return LED_MOODS[ledCurrentMood].lengthScale; }
uint16_t ledComboSpeedPercent() { return LED_MOODS[ledCurrentMood].speedPercent; }
float ledComboTargetHue()       { return ledActiveHue; }

const char *ledComboColourName() {
  if (ledLitZones < 2 || ledLitZones > 8) return "-";
  return COMBO_NAME[ledCurrentMood][ledLitZones];
}

static bool ledMoodFlashActive() {
  return ledMoodFlashUntilMs != 0 && millis() < ledMoodFlashUntilMs;
}

// =============================================================================
// HUE MATHS
// =============================================================================

static float ledComboWrap01(float v) {
  while (v < 0.0f) v += 1.0f;
  while (v >= 1.0f) v -= 1.0f;
  return v;
}

// Hue is a circle, not a line. Interpolating 0.9 -> 0.1 by simple lerp travels
// 0.9 -> 0.5 -> 0.1, sweeping backwards through green and cyan on the way.
// Going the short way round is 0.2 of travel instead of 0.8, and is what
// "converging" actually looks like.
float ledComboLerpHue(float from, float to, float blend) {
  from = ledComboWrap01(from);
  to = ledComboWrap01(to);

  float diff = to - from;
  if (diff > 0.5f)  diff -= 1.0f;
  if (diff < -0.5f) diff += 1.0f;

  return ledComboWrap01(from + diff * blend);
}

// =============================================================================
// RENDER HOOK
// =============================================================================

LedColor ledComboApply(const LedColor &color, bool active, uint8_t comboZone) {
  // Ambient and active zones are treated identically here on purpose: a dim
  // ambient zone needs FULL saturation to read as coloured at all, so there is
  // nothing to vary. Kept in the signature because it is the natural hook if
  // the two ever need to differ.
  (void)active;

  const LedMoodConfig &mood = LED_MOODS[ledCurrentMood];

  float blend = ledBlendSmoothed;
  float targetHue = ledActiveHue;

  // Only zones whose button is actually held take the combo colour. Everything
  // else keeps its own hue and carries on as normal.
  bool zoneInCombo = (comboZone < 8) && ((ledHeldMask >> comboZone) & 0x01);
  if (!zoneInCombo) blend = 0.0f;

  // The mood flash is deliberately sculpture-wide and ignores the mask - it is
  // a confirmation that Button 8 did something, so it has to be unmissable.
  if (ledMoodFlashActive()) {
    blend = 1.0f;
    targetHue = mood.signature;
  }

  // FAST PATH — nothing to do.
  //
  // In ORGANIC with this zone not in a combo, the colour comes back completely
  // untouched. Not "shifted by zero", not "blended by zero" — untouched, with
  // no float maths on the way past. This is what keeps unheld zones identical
  // to the baseline build.
  if (blend <= 0.001f && mood.hueShift == 0.0f) {
    return color;
  }

  LedColor out = color;

  // Mood hue shift stays sculpture-wide, so Button 8 is visible even with
  // nobody pressing anything.
  out.h = ledComboWrap01(out.h + mood.hueShift);

  if (blend <= 0.001f) return out;

  // Snap to the dominant colour for this zone count.
  out.h = ledComboLerpHue(out.h, targetHue, blend);

  // Saturation to full.
  //
  // Z1 (mouth) renders as white, saturation 0 - hue is meaningless on a white
  // pixel, so without this the mouth would sit unchanged while the zones
  // beside it changed colour, which looks like a fault.
  out.s = out.s + (1.0f - out.s) * blend;

  // Brightness is deliberately NOT touched. Ambient already sits near the
  // supply ceiling (see docs/power_and_ground.md).
  return out;
}

// =============================================================================
// DIAGNOSTICS
// =============================================================================

void ledComboPrintColourCode(Stream &out) {
  out.println("lit  ORGANIC          CHARGED");
  for (uint8_t n = 2; n <= 8; n++) {
    out.printf("  %u  %-8s %.2f    %-8s %.2f%s\n", n,
               COMBO_NAME[LED_MOOD_ORGANIC][n], COMBO_HUE[LED_MOOD_ORGANIC][n],
               COMBO_NAME[LED_MOOD_CHARGED][n], COMBO_HUE[LED_MOOD_CHARGED][n],
               n == ledLitZones ? "   <== now" : "");
  }
}

void ledComboPrintStatus(Stream &out) {
  const LedMoodConfig &mood = LED_MOODS[ledCurrentMood];
  out.printf("mood   : %-8s hueShift %.2f  length x%.2f  speed %u%%%s\n",
             mood.name, mood.hueShift, mood.lengthScale, mood.speedPercent,
             ledMoodFlashActive() ? "   [FLASH]" : "");
  out.printf("combo  : %u held, %u lit  hue %.2f->%.2f  blend %.2f  zones[",
             ledButtonsHeld, ledLitZones, ledActiveHue, ledTargetHue,
             ledBlendSmoothed);
  for (uint8_t z = 0; z < 8; z++) {
    out.print(((ledHeldMask >> z) & 0x01) ? (char)('1' + z) : '.');
  }
  out.printf("]  %s\n", ledLitZones >= 2 ? ledComboColourName() : "independent");
}
