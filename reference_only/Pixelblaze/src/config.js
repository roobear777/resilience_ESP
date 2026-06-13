// =============================================================================
// === CONTROL PANEL — adjust these values to tune the system ==================
// =============================================================================
// LED visual tunables only. Fire/poofer tunables live in src/fire/config.js;
// button + safety timings live in src/buttons.js and src/zone-state.js.

// --- GLOBAL ---
export var globalBrightness = 1.0

// --- ZONE 1 AMBIENT (mouth, 4-strip sequential pulse) ---
var z1amb_baseBrightness = 0.04
var z1amb_peakBrightness = 0.20
var z1amb_rampUpTimeMs = 200
var z1amb_peakTimeMs = 100
var z1amb_rampDownTimeMs = 300
var z1amb_stripStartOffsetMs = 150
var z1amb_pauseTimeMs = 600

// --- ZONE 1 ACTIVE (mouth, peristaltic wave) ---
var z1act_baseBrightness = 0.01
var z1act_peakBrightness = 0.9
var z1act_peakTimeMs = 8
var z1act_rampDownTimeMs = 600
var z1act_stripStartOffsetMs = 100
var z1act_repeatIntervalMs = 200

// --- ZONE 2 AMBIENT (shoulder, slow breathing, cyan) ---
var z2amb_hue = 0.5
var z2amb_saturation = 1.0
var z2amb_baseBrightness = 0.04
var z2amb_peakBrightness = 0.14
var z2amb_cycleDurationMs = 3000

// --- ZONE 2 ACTIVE (shoulder, peristaltic wave, cyan) ---
var z2act_baseBrightness = 0.0
var z2act_peakBrightness = 0.95
var z2act_hue = 0.5
var z2act_saturation = 1.0
var z2act_peakTimeMs = 1
var z2act_rampDownTimeMs = 800
var z2act_stripStartOffsetMs = 200
var z2act_repeatIntervalMs = 1000

// --- ZONE 3 AMBIENT (mid-body, deep breathing, blue) ---
var z3amb_hue = 0.67
var z3amb_saturation = 1.0
var z3amb_baseBrightness = 0.04
var z3amb_peakBrightness = 0.12
var z3amb_cycleDurationMs = 3300

// --- ZONE 3 ACTIVE (mid-body, center-out strobe, blue) ---
var z3act_hue = 0.67
var z3act_saturation = 1.0
var z3act_ambientBrightness = 0.08
var z3act_peakBrightness = 0.85
var z3act_strobeHz = 12
var z3act_strobeCycles = 8
var z3act_pairDelayMs = 120
var z3act_repeatIntervalMs = 1000

// --- ZONE 4 AMBIENT (rear body, slow pulse, purple) ---
var z4amb_hue = 0.917
var z4amb_saturation = 1.0
var z4amb_baseBrightness = 0.03
var z4amb_peakBrightness = 0.10
var z4amb_cycleDurationMs = 4000

// --- ZONE 4 ACTIVE (rear body, falling gradient, purple) ---
var z4act_hue = 0.917
var z4act_saturation = 1.0
var z4act_ambientBrightness = 0.02
var z4act_peakBrightness = 1.0
var z4act_gradientWidth = 8
var z4act_fallSpeedMs = 500
var z4act_stripDelayMs = 50
var z4act_repeatIntervalMs = 1500           // bumped up — must exceed full cycle

// --- ZONES 5 & 6 AMBIENT (legs, alternating pulse walk) ---
var legsAmb_hue = 0.03
var legsAmb_saturation = 1.0
var legsAmb_baseBrightness = 0.15
var legsAmb_pulsePeakBrightness = 0.35
var legsAmb_pulseDurationMs = 800
var legsAmb_cycleDurationMs = 1600

// --- ZONES 5 & 6 ACTIVE (legs, fire-zap p-wave, orange→yellow) ---
var legsAct_baseHue = 0.03
var legsAct_baseSaturation = 1.0
var legsAct_baseBrightness = 0.15
var legsAct_zapHue = 0.12
var legsAct_zapSaturation = 1.0
var legsAct_zapBrightness = 0.90
var legsAct_zapLength = 20
var legsAct_zapDurationMs = 800
var legsAct_groupDelayMs = 100

// --- ZONE 7 AMBIENT (digestive tract, falling gradient pulse, red) ---
var z7_hue = 0.0
var z7_saturation = 1.0
var z7amb_baselineBrightness = 0.25
var z7amb_peakBrightness = 1.0
var z7amb_gradientLength = 20
var z7amb_fallDurationMs = 2000
var z7amb_pauseMs = 500

// --- ZONE 7 ACTIVE (digestive tract, rapid peristalsis while button held) ---
var z7act_baselineBrightness = 0.35
var z7act_peakBrightness = 1.0
var z7act_gradientLength = 20
var z7act_fallDurationMs = 700
var z7act_pauseMs = 100

// --- ZONE 8 (button-station string lights, warm amber) ---
// 7 strings of 14 px, one per button station, running station → structure.
var z8_hue = 0.08
var z8_saturation = 0.85
var z8amb_baseBrightness = 0.06
var z8amb_peakBrightness = 0.20
var z8amb_cycleDurationMs = 4000
var z8act_baseBrightness = 0.10
var z8act_chaseBrightness = 0.95
var z8act_chaseDurationMs = 700
var z8act_tailLength = 5

// --- UI CONTROL HANDLERS ---
// Guard ignores the near-zero value PB replays for a never-touched slider
// (e.g. right after an API upload) — otherwise the whole sculpture boots black.
export function sliderGlobalBrightness(v) {
  if (v > 0.001) globalBrightness = v
}
