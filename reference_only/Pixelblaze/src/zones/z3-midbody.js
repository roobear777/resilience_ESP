// =============================================================================
// === ZONE 3 (mid-body) — 8 rings × 50 = 400
// =============================================================================
var z3amb_elapsed = 0

function z3_advanceAmbient(delta) {
  z3amb_elapsed += delta
  if (z3amb_elapsed >= z3amb_cycleDurationMs) z3amb_elapsed -= z3amb_cycleDurationMs
}

function z3_renderAmbient(localIdx) {
  var breathAmount = triangleWave(z3amb_elapsed / z3amb_cycleDurationMs)
  var brightness = z3amb_baseBrightness + (z3amb_peakBrightness - z3amb_baseBrightness) * breathAmount
  hsv(z3amb_hue, z3amb_saturation, brightness * globalBrightness)
}

var z3act_ledsPerRing = 50
var z3act_numRings = 8
var z3act_strobeHalfCycleMs = 0
var z3act_pairTotalMs = 0
var z3act_waveActive = 0
var z3act_waveElapsed = 0
var z3act_repeatTimer = 0

function z3_advanceActive(delta) {
  z3act_strobeHalfCycleMs = 500 / z3act_strobeHz
  z3act_pairTotalMs = z3act_strobeCycles * 2 * z3act_strobeHalfCycleMs
  if (z3act_waveActive == 0) {
    z3act_repeatTimer += delta
    if (z3act_repeatTimer >= z3act_repeatIntervalMs) {
      z3act_waveActive = 1
      z3act_waveElapsed = 0
      z3act_repeatTimer = 0
    }
  } else {
    z3act_waveElapsed += delta
    var waveTotalMs = 3 * z3act_pairDelayMs + z3act_pairTotalMs
    if (z3act_waveElapsed >= waveTotalMs) {
      z3act_waveActive = 0
      z3act_waveElapsed = 0
    }
  }
}

function z3_renderActive(localIdx) {
  var ring = floor(localIdx / z3act_ledsPerRing)
  if (z3act_waveActive == 0) {
    hsv(z3act_hue, z3act_saturation, z3act_ambientBrightness * globalBrightness)
    return
  }
  var distFromCenter
  if (ring == 3 || ring == 4) distFromCenter = 0
  else if (ring == 2 || ring == 5) distFromCenter = 1
  else if (ring == 1 || ring == 6) distFromCenter = 2
  else distFromCenter = 3
  var pairStartMs = distFromCenter * z3act_pairDelayMs
  var pairElapsed = z3act_waveElapsed - pairStartMs
  if (pairElapsed < 0 || pairElapsed >= z3act_pairTotalMs) {
    hsv(z3act_hue, z3act_saturation, z3act_ambientBrightness * globalBrightness)
    return
  }
  var fullCycleMs = 2 * z3act_strobeHalfCycleMs
  var cycleNum = floor(pairElapsed / fullCycleMs)
  var withinCycle = pairElapsed - cycleNum * fullCycleMs
  if (withinCycle < z3act_strobeHalfCycleMs) {
    var rampFraction = cycleNum / (z3act_strobeCycles - 1)
    var onBrightness = z3act_peakBrightness - (z3act_peakBrightness - z3act_ambientBrightness) * rampFraction
    hsv(z3act_hue, z3act_saturation, onBrightness * globalBrightness)
  } else {
    hsv(z3act_hue, z3act_saturation, z3act_ambientBrightness * globalBrightness)
  }
}
