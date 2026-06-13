// =============================================================================
// === BUTTONS — pin map, init, debounced polling ==============================
// =============================================================================

// buttonsConnected: set to 1 only after all 7 button pins have external 10kΩ
// pull-DOWN resistors to GND. Buttons are wired ACTIVE-HIGH: closing the relay
// supplies 3.3V to the pin. Without pull-downs, open relays leave pins floating
// and the system will trigger phantom presses (and phantom poofer fire).
export var buttonsConnected = 1

var debounceMs = 30

// --- PIN MAP (PixelBlaze v3.6) ---
// 16 used of 18 accessible GPIO. Reserved by PB hardware (DO NOT USE):
// IO5, IO12, IO15, IO18 (SCK), IO23 (MOSI), IO32. Spares: IO0, IO2.
var BUTTON_PINS = array(7)
BUTTON_PINS[0] = 34   // Button 1 (input-only — needs external 10kΩ pull-down to GND)
BUTTON_PINS[1] = 35   // Button 2 (input-only — needs external pull-down)
BUTTON_PINS[2] = 36   // Button 3 (input-only — needs external pull-down)
BUTTON_PINS[3] = 39   // Button 4 (input-only — needs external pull-down)
BUTTON_PINS[4] = 25   // Button 5 (top header)
BUTTON_PINS[5] = 26   // Button 6 (top header)
BUTTON_PINS[6] = 27   // Button 7

var buttonStates = array(7)
var prevButtonStates = array(7)
var rawButtonStates = array(7)
var debounceCounters = array(7)

// --- TOP-LEVEL INIT ---
// Buttons are ACTIVE-HIGH (relay supplies 3.3V when closed). All button pins
// require EXTERNAL 10kΩ pull-down resistors to GND so an open relay reads a
// clean LOW. We use plain INPUT mode — the internal pull-up MUST NOT be enabled
// here, and IO34/35/36/39 have no internal resistors at all.
for (var i = 0; i < 7; i++) {
  pinMode(BUTTON_PINS[i], INPUT)
  buttonStates[i] = 0
  prevButtonStates[i] = 0
  rawButtonStates[i] = 0
  debounceCounters[i] = 0
}

// =============================================================================
// BUTTON POLLING (with debounce)
// =============================================================================
function pollButtons(delta) {
  if (buttonsConnected == 0) {
    for (var i = 0; i < 7; i++) {
      prevButtonStates[i] = buttonStates[i]
      buttonStates[i] = 0
      rawButtonStates[i] = 0
    }
    return
  }
  for (var i = 0; i < 7; i++) {
    var raw = digitalRead(BUTTON_PINS[i])  // active high: 3.3V via relay = pressed
    if (raw != rawButtonStates[i]) {
      rawButtonStates[i] = raw
      debounceCounters[i] = 0
    } else {
      debounceCounters[i] += delta
      if (debounceCounters[i] >= debounceMs && buttonStates[i] != raw) {
        prevButtonStates[i] = buttonStates[i]
        buttonStates[i] = raw
      }
    }
  }
}
