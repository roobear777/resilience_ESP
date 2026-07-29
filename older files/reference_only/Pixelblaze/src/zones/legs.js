// =============================================================================
// === ZONES 5 & 6 (legs) — 8 logical legs × 75 LEDs
// Z5 holds logical legs 0–3, Z6 holds logical legs 4–7. Both share elapsed
// timers to stay phase-locked.
// =============================================================================
var legs_ledsPerLeg = 75
var legsAmb_elapsed = 0

function legs_advanceAmbient(delta) {
  legsAmb_elapsed += delta
  if (legsAmb_elapsed >= legsAmb_cycleDurationMs) legsAmb_elapsed -= legsAmb_cycleDurationMs
}

function legs_renderAmbient(logicalLegIdx, ledInLeg) {
  var isOddLeg = logicalLegIdx % 2
  var pulseStartMs
  if (isOddLeg) pulseStartMs = 0
  else pulseStartMs = legsAmb_cycleDurationMs / 2
  var timeSincePulseStart = legsAmb_elapsed - pulseStartMs
  if (timeSincePulseStart < 0) timeSincePulseStart += legsAmb_cycleDurationMs
  var brightness = legsAmb_baseBrightness
  if (timeSincePulseStart < legsAmb_pulseDurationMs) {
    var pulseAmount = triangleWave(timeSincePulseStart / legsAmb_pulseDurationMs)
    brightness = legsAmb_baseBrightness + (legsAmb_pulsePeakBrightness - legsAmb_baseBrightness) * pulseAmount
  }
  hsv(legsAmb_hue, legsAmb_saturation, brightness * globalBrightness)
}

var legsAct_elapsed = 0
var legsAct_pauseMs = 300

function legs_advanceActive(delta) {
  legsAct_elapsed += delta
  var cycleTotal = legsAct_groupDelayMs + legsAct_zapDurationMs + legsAct_zapLength * (legsAct_zapDurationMs / legs_ledsPerLeg) + legsAct_pauseMs
  if (legsAct_elapsed >= cycleTotal) legsAct_elapsed -= cycleTotal
}

function legs_renderActive(logicalLegIdx, ledInLeg) {
  var isOddLeg = logicalLegIdx % 2
  var legStartMs
  if (isOddLeg) legStartMs = 0
  else legStartMs = legsAct_groupDelayMs

  var legElapsed = legsAct_elapsed - legStartMs

  // Peak position sweeps from -zapLength to ledsPerLeg over zapDurationMs.
  // This lets the gradient enter cleanly from before LED 0 and exit cleanly
  // past the end of the strip.
  var peakPos = (legElapsed / legsAct_zapDurationMs) * (legs_ledsPerLeg + legsAct_zapLength) - legsAct_zapLength

  var brightness = legsAct_baseBrightness
  var distBehind = peakPos - ledInLeg

  if (legElapsed >= 0 && distBehind >= 0 && distBehind <= legsAct_zapLength) {
    // Trailing gradient: ramp from peak (distBehind=0) to base (distBehind=zapLength)
    var rampFraction = distBehind / legsAct_zapLength
    brightness = legsAct_zapBrightness - (legsAct_zapBrightness - legsAct_baseBrightness) * rampFraction
  }

  if (brightness > legsAct_baseBrightness) {
    var blendFraction = (brightness - legsAct_baseBrightness) / (legsAct_zapBrightness - legsAct_baseBrightness)
    var h = legsAct_baseHue + (legsAct_zapHue - legsAct_baseHue) * blendFraction
    hsv(h, legsAct_zapSaturation, brightness * globalBrightness)
  } else {
    hsv(legsAct_baseHue, legsAct_baseSaturation, legsAct_baseBrightness * globalBrightness)
  }
}
