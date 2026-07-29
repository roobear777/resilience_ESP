// =============================================================================
// PER-FRAME LOGIC
// =============================================================================
// Lines tagged //#fire are stripped by `build.js --led-only`.
export function beforeRender(delta) {
  pollButtons(delta)
  updateZoneStates(delta)
  for (var i = 0; i < 7; i++) zoneTime[i] += delta

  z1_advanceAmbient(delta)
  z1_advanceActive(delta)
  z2_advanceAmbient(delta)
  z2_advanceActive(delta)
  z3_advanceAmbient(delta)
  z3_advanceActive(delta)
  z4_advanceAmbient(delta)
  z4_advanceActive(delta)
  legs_advanceAmbient(delta)
  legs_advanceActive(delta)
  z7_advanceAmbient(delta)
  z7_advanceActive(delta)
  z8_advance(delta)

  updatePoofers(delta)  //#fire
  writePooferPins()     //#fire
}


// =============================================================================
// PIXEL DISPATCH
// =============================================================================
export function render(index) {
  if (index < ZONE_OFFSETS[1]) {
    var localIdx = index
    if (zoneActive[0]) z1_renderActive(localIdx)
    else               z1_renderAmbient(localIdx)
  } else if (index < ZONE_OFFSETS[2]) {
    var localIdx = index - ZONE_OFFSETS[1]
    if (zoneActive[1]) z2_renderActive(localIdx)
    else               z2_renderAmbient(localIdx)
  } else if (index < ZONE_OFFSETS[3]) {
    var localIdx = index - ZONE_OFFSETS[2]
    if (zoneActive[2]) z3_renderActive(localIdx)
    else               z3_renderAmbient(localIdx)
  } else if (index < ZONE_OFFSETS[4]) {
    var localIdx = index - ZONE_OFFSETS[3]
    if (zoneActive[3]) z4_renderActive(localIdx)
    else               z4_renderAmbient(localIdx)
  } else if (index < ZONE_OFFSETS[5]) {
    var localIdx = index - ZONE_OFFSETS[4]
    var legIdx = floor(localIdx / legs_ledsPerLeg)
    var ledInLeg = localIdx % legs_ledsPerLeg
    if (zoneActive[4]) legs_renderActive(legIdx, ledInLeg)
    else               legs_renderAmbient(legIdx, ledInLeg)
  } else if (index < ZONE_OFFSETS[6]) {
    var localIdx = index - ZONE_OFFSETS[5]
    var legIdx = floor(localIdx / legs_ledsPerLeg) + 4
    var ledInLeg = localIdx % legs_ledsPerLeg
    if (zoneActive[5]) legs_renderActive(legIdx, ledInLeg)
    else               legs_renderAmbient(legIdx, ledInLeg)
  } else if (index < ZONE_OFFSETS[7]) {
    var localIdx = index - ZONE_OFFSETS[6]
    if (zoneActive[6]) z7_renderActive(localIdx)
    else               z7_renderAmbient(localIdx)
  } else {
    // Z8: each string follows its own station's button
    var localIdx = index - ZONE_OFFSETS[7]
    var station = z8_stationFor(localIdx)
    var posInString = localIdx % z8_pixelsPerString
    if (zoneActive[station]) z8_renderActive(station, posInString)
    else                     z8_renderAmbient(station, posInString)
  }
}
