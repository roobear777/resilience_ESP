// =============================================================================
// === ZONE STATE — hold timers, safety latches, payoff detection ==============
// =============================================================================
// Interactivity model: each of the 7 buttons independently activates its zone.
// Holding all 7 simultaneously layers on the "payoff" effects.

// --- SAFETY (code-only, NOT exposed to UI to prevent accidental disable) ---
var zoneMaxHoldMs = 10000                   // a single zone force-latches off after this long held
var payoffMaxHoldMs = 10000                 // the all-7 payoff force-latches off after this long held

// --- STATE ---
var zoneActive = array(7)        // 1 = this zone's button is held and zone is live
var zoneHoldMs = array(7)        // ms this zone's button has been continuously held
var zoneLatched = array(7)       // 1 = zone hit its 10s cap, locked off until release
var allSevenHeld = 0             // 1 = all 7 buttons currently held
var payoffHoldMs = 0             // ms the all-7 payoff state has been continuously held
var payoffLatched = 0            // 1 = payoff hit its 10s cap, locked off until release

var zoneTime = array(7)

for (var i = 0; i < 7; i++) {
  zoneTime[i] = 0
  zoneActive[i] = 0
  zoneHoldMs[i] = 0
  zoneLatched[i] = 0
}

// =============================================================================
// ZONE ACTIVATION LOGIC (independent per-zone model)
// =============================================================================
// Each button independently activates its own zone. A zone stays active as long
// as its button is held, EXCEPT it force-latches off after zoneMaxHoldMs of
// continuous holding (fire safety against a wedged button). A latched zone
// stays off until its button is physically released.
//
// Holding all 7 buttons triggers the "payoff" — extra effects layered on top.
// The payoff also force-latches after payoffMaxHoldMs.
// =============================================================================
function updateZoneStates(delta) {
  for (var i = 0; i < 7; i++) {
    var held = buttonStates[i]

    if (held) {
      zoneHoldMs[i] += delta
      // Fire-safety cap: latch the zone off after the max hold time
      if (zoneHoldMs[i] >= zoneMaxHoldMs) {
        zoneLatched[i] = 1
      }
    } else {
      // Button released — clear hold timer and latch
      zoneHoldMs[i] = 0
      zoneLatched[i] = 0
    }

    // Zone is active only if held AND not latched off
    if (held && !zoneLatched[i]) {
      zoneActive[i] = 1
    } else {
      zoneActive[i] = 0
    }
  }

  // All-7 detection: every zone's button held (latched zones still count as
  // "held" for this check — the payoff latch is handled separately below)
  var allHeld = 1
  for (var i = 0; i < 7; i++) {
    if (!buttonStates[i]) allHeld = 0
  }

  if (allHeld) {
    payoffHoldMs += delta
    if (payoffHoldMs >= payoffMaxHoldMs) {
      payoffLatched = 1
    }
  } else {
    payoffHoldMs = 0
    payoffLatched = 0
  }

  // Payoff is live only if all 7 held AND not latched off
  if (allHeld && !payoffLatched) {
    allSevenHeld = 1
  } else {
    allSevenHeld = 0
  }
}
