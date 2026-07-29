// =============================================================================
// === ZONE 2 (shoulder) — 6 tapered strips: 44, 48, 52, 56, 60, 65 = 325
// =============================================================================
var z2amb_elapsed = 0

function z2_advanceAmbient(delta) {
  z2amb_elapsed += delta
  if (z2amb_elapsed >= z2amb_cycleDurationMs) z2amb_elapsed -= z2amb_cycleDurationMs
}

function z2_renderAmbient(localIdx) {
  var breathAmount = triangleWave(z2amb_elapsed / z2amb_cycleDurationMs)
  var brightness = z2amb_baseBrightness + (z2amb_peakBrightness - z2amb_baseBrightness) * breathAmount
  hsv(z2amb_hue, z2amb_saturation, brightness * globalBrightness)
}

var z2act_numStrips = 6
var z2act_cycle1Elapsed = -1
var z2act_cycle2Elapsed = -1
var z2act_repeatTimer = 0

function z2act_stripLength(s) {
  if (s == 0) return 44
  if (s == 1) return 48
  if (s == 2) return 52
  if (s == 3) return 56
  if (s == 4) return 60
  return 65
}

function z2_advanceActive(delta) {
  var fullCycleDuration = (z2act_numStrips - 1) * z2act_stripStartOffsetMs + z2act_peakTimeMs + z2act_rampDownTimeMs
  if (z2act_cycle1Elapsed >= 0) {
    z2act_cycle1Elapsed += delta
    if (z2act_cycle1Elapsed > fullCycleDuration) z2act_cycle1Elapsed = -1
  }
  if (z2act_cycle2Elapsed >= 0) {
    z2act_cycle2Elapsed += delta
    if (z2act_cycle2Elapsed > fullCycleDuration) z2act_cycle2Elapsed = -1
  }
  z2act_repeatTimer += delta
  if (z2act_repeatTimer >= z2act_repeatIntervalMs) {
    z2act_repeatTimer = 0
    if (z2act_cycle1Elapsed < 0) z2act_cycle1Elapsed = 0
    else if (z2act_cycle2Elapsed < 0) z2act_cycle2Elapsed = 0
  }
}

function z2act_cycleBrightness(cycleElapsed, stripIdx) {
  if (cycleElapsed < 0) return -1
  var timeSinceActivation = cycleElapsed - stripIdx * z2act_stripStartOffsetMs
  return peakRampBrightness(timeSinceActivation, z2act_peakTimeMs, z2act_rampDownTimeMs, z2act_peakBrightness, z2act_baseBrightness)
}

function z2_renderActive(localIdx) {
  var stripIdx = 0
  var cumulative = 0
  var s = 0
  while (s < z2act_numStrips) {
    var len = z2act_stripLength(s)
    if (localIdx < cumulative + len) {
      stripIdx = s
      s = z2act_numStrips
    } else {
      cumulative += len
      s += 1
    }
  }
  var b1 = z2act_cycleBrightness(z2act_cycle1Elapsed, stripIdx)
  var b2 = z2act_cycleBrightness(z2act_cycle2Elapsed, stripIdx)
  var brightness = z2act_baseBrightness
  if (b1 > brightness) brightness = b1
  if (b2 > brightness) brightness = b2
  hsv(z2act_hue, z2act_saturation, brightness * globalBrightness)
}
