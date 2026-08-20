#ifndef ECLAIR_LED_COMBO_H
#define ECLAIR_LED_COMBO_H

#include <Arduino.h>

#include "led_color.h"

// =============================================================================
// COMBO LIGHTING — two moods, and convergence driven by how many buttons
// =============================================================================
//
// Two independent axes:
//
//   MOOD  (Button 8)     what kind of creature it is
//   COUNT (how many held) how unified it is
//
// -----------------------------------------------------------------------------
// WHY COUNT AND NOT SPECIFIC PAIRS
// -----------------------------------------------------------------------------
//
// The buttons are at seven separate stations, so any combination requires more
// than one person. Driving effects off HOW MANY buttons are held rather than
// WHICH means:
//
//   - nobody has to memorise or be told anything
//   - the sculpture visibly rewards more people joining in
//   - accidental pairs (inevitable with seven strangers) still look intentional
//
// With 8 buttons there are 28 possible pairs. Mapping those to specific effects
// would mean most triggers are accidental and nobody connects cause to effect.
//
// -----------------------------------------------------------------------------
// WHAT CONVERGENCE DOES
// -----------------------------------------------------------------------------
//
// Each zone normally has its own hue — white mouth, cyan shoulder, blue
// midbody, purple rear, orange legs, red gut. As more buttons are held, those
// hues pull toward a single shared target:
//
//   1 lit     that zone animates in its own colour, as always
//   2 lit     the held zones snap to one shared colour
//   3         a DIFFERENT shared colour
//   4         ... and so on. EVERY count has its own clearly separated colour.
//
// The colour is chosen by how many ZONES ARE LIT, not how many buttons are
// down. Button 8 has no zone, so counting buttons meant holding B8 with three
// zone buttons showed the four-zone colour while only three zones were lit.
//
// SCOPE: only the zones whose buttons are actually held change colour. Press
// buttons 2 and 3 and Z2 and Z3 turn green together; every other zone carries
// on in its own colour, undisturbed. The people touching the sculpture see
// their own zones respond, and see them MATCH each other, which is the point.
//
// (An earlier version recoloured the entire sculpture on any two-button press.
// That drowned out the connection between what you pressed and what changed.)
//
// It is a COLOUR CODE, not a gradual blend. Halfway convergence was not
// visible from any useful distance - the zones still looked different from
// each other, so it read as "nothing happened". Snapping to one shared colour
// is legible even at night.
//
// Each MOOD has its own colour set, which is also what makes Button 8 obvious:
// the same two-button press gives a cool colour in ORGANIC and a warm one in
// CHARGED.
//
// IMPORTANT: convergence only ever moves HUE and SATURATION. Never brightness.
// Ambient draw already sits near the supply ceiling (see docs/power_and_ground)
// so "everything gets brighter" is not available to us. Concentrating and
// unifying colour reads as more dramatic than adding light anyway.
//
// -----------------------------------------------------------------------------
// WHAT THE MOOD DOES
// -----------------------------------------------------------------------------
//
// Button 8 has no zone of its own (7 body zones, 8 buttons) and only 7 station
// strings exist, so it has no path either. Instead it toggles the whole
// sculpture between two personalities.
//
// The zones are shaped two different ways, which is why the mood changes both
// speed and length — either one alone would leave half the sculpture unmoved:
//
//   TIME-SHAPED  Z1 mouth, Z2 shoulder, Z3 midbody
//                character comes from timing -> responds to SPEED
//
//   SPACE-SHAPED Z4 rear, Z5/Z6 legs, Z7 digestive, Z8 stations
//                character comes from how many pixels are lit at once
//                -> responds to LENGTH
//
// =============================================================================

enum LedMood {
  LED_MOOD_ORGANIC = 0,   // EXACT no-op at rest: the sculpture as built. Cool combo colours.
  LED_MOOD_CHARGED = 1,   // hue rotated, 1.6x speed, tight tails. Warm combo colours.
  LED_MOOD_COUNT
};

// Toggling the mood also fires a brief full-sculpture flash in the new mood's
// signature colour, so a B8 press is unmistakable rather than something you
// have to go looking for.

// --- called from the sketch ---
void ledComboBegin();
// mask: bit N set = zone ZN+1's button is held. Only those zones take the
// combo colour. count drives WHICH colour.
void ledComboSetButtonsHeld(uint8_t count, uint8_t heldZoneMask);
void ledComboToggleMood();
void ledComboSetMood(LedMood mood);

// --- state ---
LedMood ledComboMood();
const char *ledComboMoodName();
uint8_t ledComboButtonsHeld();
uint8_t ledComboHeldMask();
uint8_t ledComboLitZones();     // popcount(mask) - this drives WHICH colour
float ledComboBlend();          // 0..1 convergence amount
float ledComboLengthScale();    // multiplier for space-shaped zone lengths
uint16_t ledComboSpeedPercent(); // animation speed, percent (100 = unchanged)
float ledComboTargetHue();      // dominant colour for the current count
const char *ledComboColourName();
void ledComboPrintColourCode(Stream &out);

// --- render path ---
// Applied AFTER a zone has rendered its own colour, so the animation keeps
// running. This is the difference from the old all-green override, which
// returned before any zone code ran and left the sculpture looking broken
// rather than transformed.
// comboZone: which zone this pixel belongs to for combo purposes. For the Z8
// station strings this is the STATION's zone, not 7, so a station string
// changes colour along with the body zone its button drives.
LedColor ledComboApply(const LedColor &color, bool active, uint8_t comboZone);

// Shortest-arc hue interpolation. Going 0.9 -> 0.1 the naive way sweeps
// BACKWARDS through every colour on the wheel; this wraps the short way.
float ledComboLerpHue(float from, float to, float blend);

void ledComboPrintStatus(Stream &out);

#endif
