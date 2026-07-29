// =============================================================================
// === ZONE 1 (mouth) — 4 tapered strips: 33, 50, 60, 65 = 208
// =============================================================================
var z1_numStrips = 4
function z1_stripLength(s) {
  if (s == 0) return 33
  if (s == 1) return 50
  if (s == 2) return 60
  return 65
}

// Resolve a localIdx to its strip index (shared by ambient and active)
function z1_stripIdxFor(localIdx) {
  var cumulative = 0
  var s = 0
  while (s < z1_numStrips) {
    var len = z1_stripLength(s)
    if (localIdx < cumulative + len) return s
    cumulative += len
    s += 1
  }
  return z1_numStrips - 1
}

// --- Z1 AMBIENT (new 4-strip sequential pulse) ---
var z1amb_elapsed = 0

function z1_advanceAmbient(delta) {
  z1amb_elapsed += delta
  var ringDuration = z1amb_rampUpTimeMs + z1amb_peakTimeMs + z1amb_rampDownTimeMs
  var fullCycle = (z1_numStrips - 1) * z1amb_stripStartOffsetMs + ringDuration + z1amb_pauseTimeMs
  if (z1amb_elapsed >= fullCycle) z1amb_elapsed -= fullCycle
}

function z1_renderAmbient(localIdx) {
  var stripIdx = z1_stripIdxFor(localIdx)
  var stripStartMs = stripIdx * z1amb_stripStartOffsetMs
  var t = z1amb_elapsed - stripStartMs
  var brightness = z1amb_baseBrightness
  if (t >= 0) {
    if (t < z1amb_rampUpTimeMs) {
      brightness = z1amb_baseBrightness + (z1amb_peakBrightness - z1amb_baseBrightness) * (t / z1amb_rampUpTimeMs)
    } else if (t < z1amb_rampUpTimeMs + z1amb_peakTimeMs) {
      brightness = z1amb_peakBrightness
    } else if (t < z1amb_rampUpTimeMs + z1amb_peakTimeMs + z1amb_rampDownTimeMs) {
      var rdT = t - z1amb_rampUpTimeMs - z1amb_peakTimeMs
      brightness = z1amb_peakBrightness - (z1amb_peakBrightness - z1amb_baseBrightness) * (rdT / z1amb_rampDownTimeMs)
    }
  }
  hsv(0, 0, brightness * globalBrightness)
}

// --- Z1 ACTIVE (4-strip peristaltic wave) ---
var z1act_elapsed = 0

function z1_advanceActive(delta) {
  z1act_elapsed += delta
  var fullDuration = (z1_numStrips - 1) * z1act_stripStartOffsetMs + z1act_peakTimeMs + z1act_rampDownTimeMs
  var cycleLength = fullDuration
  if (z1act_repeatIntervalMs > cycleLength) cycleLength = z1act_repeatIntervalMs
  if (z1act_elapsed >= cycleLength) z1act_elapsed -= cycleLength
}

function z1_renderActive(localIdx) {
  var stripIdx = z1_stripIdxFor(localIdx)
  var stripActivationMs = stripIdx * z1act_stripStartOffsetMs
  var timeSinceActivation = z1act_elapsed - stripActivationMs
  var brightness = peakRampBrightness(timeSinceActivation, z1act_peakTimeMs, z1act_rampDownTimeMs, z1act_peakBrightness, z1act_baseBrightness)
  if (brightness < 0) brightness = z1act_baseBrightness
  hsv(0, 0, brightness * globalBrightness)
}
