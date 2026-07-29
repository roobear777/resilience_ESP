// =============================================================================
// === ZONE 7 (digestive tract) — 75-LED strand × 7 parallel
// Baseline glow + gradient pulse falling top→bottom (animation runs visually
// top→bottom; physical wiring runs bottom→top so we reverse the index).
// Ambient = slow pulse with pause. Active (button held) = faster, brighter
// peristalsis using the same shape.
// =============================================================================
var z7_strandLength = 75

// Shared pulse shape: triangle gradient of `gradientLength` pixels falling
// down the strand over `fallDurationMs`. Returns brightness for this pixel.
function z7_pulseBrightness(localIdx, elapsed, fallDurationMs, gradientLength, baselineBrightness, peakBrightness) {
  var pos = localIdx % z7_strandLength
  var visualPos = z7_strandLength - 1 - pos
  var brightness = baselineBrightness
  if (elapsed < fallDurationMs) {
    var peakPos = (elapsed / fallDurationMs) * (z7_strandLength + gradientLength) - gradientLength
    var distFromPeak = visualPos - peakPos
    if (distFromPeak >= 0 && distFromPeak <= gradientLength) {
      var halfWidth = gradientLength / 2
      var rampUp
      if (distFromPeak < halfWidth) {
        rampUp = distFromPeak / halfWidth
      } else {
        rampUp = (gradientLength - distFromPeak) / halfWidth
      }
      brightness = baselineBrightness + (peakBrightness - baselineBrightness) * rampUp
    }
  }
  return brightness
}

// --- Z7 AMBIENT (slow falling pulse, always running) ---
var z7amb_elapsed = 0

function z7_advanceAmbient(delta) {
  z7amb_elapsed += delta
  var cycleTotal = z7amb_fallDurationMs + z7amb_pauseMs
  if (z7amb_elapsed >= cycleTotal) z7amb_elapsed -= cycleTotal
}

function z7_renderAmbient(localIdx) {
  var brightness = z7_pulseBrightness(localIdx, z7amb_elapsed,
    z7amb_fallDurationMs, z7amb_gradientLength,
    z7amb_baselineBrightness, z7amb_peakBrightness)
  hsv(z7_hue, z7_saturation, brightness * globalBrightness)
}

// --- Z7 ACTIVE (rapid peristalsis while button held) ---
var z7act_elapsed = 0

function z7_advanceActive(delta) {
  z7act_elapsed += delta
  var cycleTotal = z7act_fallDurationMs + z7act_pauseMs
  if (z7act_elapsed >= cycleTotal) z7act_elapsed -= cycleTotal
}

function z7_renderActive(localIdx) {
  var brightness = z7_pulseBrightness(localIdx, z7act_elapsed,
    z7act_fallDurationMs, z7act_gradientLength,
    z7act_baselineBrightness, z7act_peakBrightness)
  hsv(z7_hue, z7_saturation, brightness * globalBrightness)
}
