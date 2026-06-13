// =============================================================================
// === ZONE 8 (button-station string lights) — 7 strings × 14 px = 98 (+2 spare)
// One string per button station, physically running from the station toward
// the main structure. OE Ch 0, start index 1908.
// Ambient = gentle warm breathing, phase-staggered per station.
// Active (that station's button held) = bright chase flowing station → body.
// =============================================================================
var z8_pixelsPerString = 14
var z8_numStations = 7
var z8amb_elapsed = 0
var z8act_elapsed = 0

// Map a Z8-local pixel index to its station (clamps the 2 spare pixels onto
// the last string so nothing renders out of range)
function z8_stationFor(localIdx) {
  var station = floor(localIdx / z8_pixelsPerString)
  if (station >= z8_numStations) station = z8_numStations - 1
  return station
}

function z8_advance(delta) {
  z8amb_elapsed += delta
  if (z8amb_elapsed >= z8amb_cycleDurationMs) z8amb_elapsed -= z8amb_cycleDurationMs
  z8act_elapsed += delta
  if (z8act_elapsed >= z8act_chaseDurationMs) z8act_elapsed -= z8act_chaseDurationMs
}

function z8_renderAmbient(station, posInString) {
  // Stagger each station's breathing phase so the ring of stations shimmers
  var progress = z8amb_elapsed / z8amb_cycleDurationMs + station / z8_numStations
  progress = progress % 1
  var breathAmount = triangleWave(progress)
  var brightness = z8amb_baseBrightness + (z8amb_peakBrightness - z8amb_baseBrightness) * breathAmount
  hsv(z8_hue, z8_saturation, brightness * globalBrightness)
}

function z8_renderActive(station, posInString) {
  // Chase head sweeps 0 → pixelsPerString; trailing gradient behind it
  var headPos = (z8act_elapsed / z8act_chaseDurationMs) * (z8_pixelsPerString + z8act_tailLength)
  var distBehind = headPos - posInString
  var brightness = z8act_baseBrightness
  if (distBehind >= 0 && distBehind <= z8act_tailLength) {
    var rampFraction = distBehind / z8act_tailLength
    brightness = z8act_chaseBrightness - (z8act_chaseBrightness - z8act_baseBrightness) * rampFraction
  }
  hsv(z8_hue, z8_saturation, brightness * globalBrightness)
}
