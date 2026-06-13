// =============================================================================
// === SHARED ANIMATION PRIMITIVES =============================================
// =============================================================================
// Conservative helpers for shapes duplicated across zones. PB language has no
// objects/closures, so these take scalar args; per-zone state stays in the
// zone modules.

// Triangle wave: 0 → 1 → 0 as progress goes 0 → 1.
// Used by every breathing/pulse animation (Z2/Z3/Z4 ambient, legs, Z8).
function triangleWave(progress) {
  if (progress < 0.5) return progress * 2
  return (1 - progress) * 2
}

// Peristaltic envelope: hold at peakBrightness for peakTimeMs, then ramp
// linearly down to baseBrightness over rampDownTimeMs. Returns -1 when t is
// outside the envelope (caller decides the fallback brightness).
// Used by the Z1 and Z2 active strip waves.
function peakRampBrightness(t, peakTimeMs, rampDownTimeMs, peakBrightness, baseBrightness) {
  if (t < 0) return -1
  if (t >= peakTimeMs + rampDownTimeMs) return -1
  if (t < peakTimeMs) return peakBrightness
  var timeInRampDown = t - peakTimeMs
  var b = peakBrightness - ((peakBrightness - baseBrightness) * (timeInRampDown / rampDownTimeMs))
  if (b < baseBrightness) b = baseBrightness
  return b
}
