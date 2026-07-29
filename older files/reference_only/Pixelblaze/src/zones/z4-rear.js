// =============================================================================
// === ZONE 4 (rear body) — 12 strips × 25 = 300
// =============================================================================
var z4amb_elapsed = 0

function z4_advanceAmbient(delta) {
  z4amb_elapsed += delta
  if (z4amb_elapsed >= z4amb_cycleDurationMs) z4amb_elapsed -= z4amb_cycleDurationMs
}

function z4_renderAmbient(localIdx) {
  var breathAmount = triangleWave(z4amb_elapsed / z4amb_cycleDurationMs)
  var brightness = z4amb_baseBrightness + (z4amb_peakBrightness - z4amb_baseBrightness) * breathAmount
  hsv(z4amb_hue, z4amb_saturation, brightness * globalBrightness)
}

var z4act_ledsPerStrip = 25
var z4act_numStrips = 12
var z4act_cycle1Elapsed = -1
var z4act_cycle2Elapsed = -1
var z4act_repeatTimer = 0
var z4act_centerA = floor((z4act_numStrips - 1) / 2)  // 5
var z4act_centerB = floor(z4act_numStrips / 2)        // 6
var z4act_maxDist = z4act_centerA                     // 5
var z4act_fullFallMs = z4act_fallSpeedMs * (z4act_ledsPerStrip + z4act_gradientWidth) / z4act_ledsPerStrip

function z4_advanceActive(delta) {
  z4act_centerA = floor((z4act_numStrips - 1) / 2)
  z4act_centerB = floor(z4act_numStrips / 2)
  z4act_maxDist = z4act_centerA
  z4act_fullFallMs = z4act_fallSpeedMs * (z4act_ledsPerStrip + z4act_gradientWidth) / z4act_ledsPerStrip
  var oneCycleTotalMs = z4act_maxDist * z4act_stripDelayMs + z4act_fullFallMs
  if (z4act_cycle1Elapsed >= 0) {
    z4act_cycle1Elapsed += delta
    if (z4act_cycle1Elapsed > oneCycleTotalMs) z4act_cycle1Elapsed = -1
  }
  if (z4act_cycle2Elapsed >= 0) {
    z4act_cycle2Elapsed += delta
    if (z4act_cycle2Elapsed > oneCycleTotalMs) z4act_cycle2Elapsed = -1
  }
  z4act_repeatTimer += delta
  if (z4act_repeatTimer >= z4act_repeatIntervalMs) {
    z4act_repeatTimer = 0
    if (z4act_cycle1Elapsed < 0) z4act_cycle1Elapsed = 0
    else if (z4act_cycle2Elapsed < 0) z4act_cycle2Elapsed = 0
  }
}

function z4act_cycleBrightness(cycleElapsed, stripIdx, ledInStrip) {
  if (cycleElapsed < 0) return -1
  var distFromCenter
  if (stripIdx <= z4act_centerA) distFromCenter = z4act_centerA - stripIdx
  else distFromCenter = stripIdx - z4act_centerB
  var stripStartMs = distFromCenter * z4act_stripDelayMs
  var stripElapsed = cycleElapsed - stripStartMs
  if (stripElapsed < 0) return -1
  var peakLED = floor((stripElapsed / z4act_fallSpeedMs) * z4act_ledsPerStrip)
  if (peakLED > z4act_ledsPerStrip + z4act_gradientWidth) return -1
  if (ledInStrip == peakLED) return z4act_peakBrightness
  var distBehind = peakLED - ledInStrip
  if (distBehind > 0 && distBehind <= z4act_gradientWidth) {
    var rampFraction = distBehind / z4act_gradientWidth
    return z4act_peakBrightness - (z4act_peakBrightness - z4act_ambientBrightness) * rampFraction
  }
  return -1
}

function z4_renderActive(localIdx) {
  var stripIdx = floor(localIdx / z4act_ledsPerStrip)
  var ledInStrip = localIdx % z4act_ledsPerStrip
  var b1 = z4act_cycleBrightness(z4act_cycle1Elapsed, stripIdx, ledInStrip)
  var b2 = z4act_cycleBrightness(z4act_cycle2Elapsed, stripIdx, ledInStrip)
  var brightness = z4act_ambientBrightness
  if (b1 > brightness) brightness = b1
  if (b2 > brightness) brightness = b2
  hsv(z4act_hue, z4act_saturation, brightness * globalBrightness)
}
