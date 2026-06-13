// =============================================================================
// PER-FRAME LOGIC — PRESSURE TEST
// =============================================================================
// Burn-in entry point: every zone runs its ACTIVE animation continuously.
// No buttons, no state machine, no fire. Draws significantly more power than
// the interactive installation — use for bench/burn-in testing only.
export function beforeRender(delta) {
  z1_advanceActive(delta)
  z2_advanceActive(delta)
  z3_advanceActive(delta)
  z4_advanceActive(delta)
  legs_advanceActive(delta)
  z7_advanceActive(delta)
  z8_advance(delta)
}


// =============================================================================
// PIXEL DISPATCH — all zones forced active
// =============================================================================
export function render(index) {
  if (index < ZONE_OFFSETS[1]) {
    z1_renderActive(index)
  } else if (index < ZONE_OFFSETS[2]) {
    z2_renderActive(index - ZONE_OFFSETS[1])
  } else if (index < ZONE_OFFSETS[3]) {
    z3_renderActive(index - ZONE_OFFSETS[2])
  } else if (index < ZONE_OFFSETS[4]) {
    z4_renderActive(index - ZONE_OFFSETS[3])
  } else if (index < ZONE_OFFSETS[5]) {
    var localIdx = index - ZONE_OFFSETS[4]
    legs_renderActive(floor(localIdx / legs_ledsPerLeg), localIdx % legs_ledsPerLeg)
  } else if (index < ZONE_OFFSETS[6]) {
    var localIdx = index - ZONE_OFFSETS[5]
    legs_renderActive(floor(localIdx / legs_ledsPerLeg) + 4, localIdx % legs_ledsPerLeg)
  } else if (index < ZONE_OFFSETS[7]) {
    z7_renderActive(index - ZONE_OFFSETS[6])
  } else {
    var localIdx = index - ZONE_OFFSETS[7]
    z8_renderActive(z8_stationFor(localIdx), localIdx % z8_pixelsPerString)
  }
}
