// =============================================================================
// TARDI — FIRE CONTROLLER
// ESP32-S3, Resilience tardigrade build
// =============================================================================
//
// One half of the two-board split. Tardi does buttons, fire and the OLED.
// That is the whole program.
//
//   - 8 debounced active-HIGH button inputs
//   - FIRE1-FIRE9, active-LOW (HIGH = idle, LOW = trigger)
//   - FIRE1-FIRE8 pulse while their button is held
//   - FIRE9 / Head Poof while Button 1 + Button 8 are held, with a hard cutoff
//   - SSD1306 OLED status
//   - USB serial diagnostics
//
// -----------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT HERE
// -----------------------------------------------------------------------------
//
// No WiFi. No web server. No LED animation engine. No DMA. No 22 ms frame
// loop. Those all live on Eclair, the LED board, on the other end of the
// button wires.
//
// This matters. In the single-board firmware, one ESP32 ran WiFi, an HTTP
// server, an OLED, ~2,000 pixels of DMA output and nine fire relays. A crash
// or a stalled frame loop took fire control with it, and the LED write blocked
// the loop for tens of milliseconds at a time — which is exactly how long a
// solenoid stays open past when it should have closed.
//
// Keeping this board dumb and offline is the single biggest reliability gain
// available in the whole project. Please do not add features to it.
//
// -----------------------------------------------------------------------------
// HOW THE TWO BOARDS STAY IN SYNC
// -----------------------------------------------------------------------------
//
// They don't talk to each other. The same physical button wires land on the
// SAME GPIO numbers on both boards. Both see the same edge at the same
// instant. There is no link to fail — if a button stops working, it stops
// working on both boards identically, and a multimeter finds it in seconds.
//
// -----------------------------------------------------------------------------
// ARDUINO IDE
// -----------------------------------------------------------------------------
//   Board            : ESP32S3 Dev Module
//   PSRAM            : OPI PSRAM
//   Flash Size       : 8MB
//   USB CDC On Boot  : Enabled (recommended) or Disabled — either works here,
//                      because Tardi drives no LEDs and UART0 (GPIO43/44) is
//                      free. This is one of the pins the split gave back.
//   Libraries        : Adafruit SSD1306, Adafruit GFX
// =============================================================================

#include <Arduino.h>

// ==================================================
// OLED HARDWARE SWITCH
// ==================================================
// 0 = OLED code compiled out (use before the display is wired)
// 1 = OLED code compiled in. Needs Adafruit SSD1306 + Adafruit GFX.
#define ENABLE_OLED_HARDWARE 1

#if ENABLE_OLED_HARDWARE
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

// =============================================================================
// CONFIG
// =============================================================================

const int NUM_BUTTONS = 8;
const int NUM_FIRE_OUTPUTS = 9;
const int FIRE9_INDEX = 8;

const unsigned long DEBOUNCE_MS = 30;

// FIRE1-FIRE8: pulse this long, repeating while the button is held.
//
// NOTE: the old README and current_baseline.md both said 500 ms while the
// code said 100 ms. 100 ms is what actually ran, so 100 ms is what stays —
// but the docs need correcting, not this line.
const unsigned long NORMAL_FIRE_PULSE_MS = 100;
const unsigned long FIRE_REPEAT_INTERVAL_MS = 1000;

// =============================================================================
// BIG POOF  (FIRE9)
// =============================================================================
//
//   all 8 buttons held  ->  every zone poofer fires TOGETHER for
//                          BIG_POOF_ZONE_MS, and FIRE9 for
//                          BIG_POOF_DURATION_MS
//                       ->  then EVERY poofer locked out for
//                          BIG_POOF_COOLDOWN_MS
//
//     t = 0.0 s   FIRE1..FIRE8 all on together, FIRE9 on
//     t = 1.0 s   FIRE1..FIRE8 all off
//     t = 1.5 s   FIRE9 off, lockout begins
//     t = 31.5 s  lockout ends
//
// WHY THE ZONES ARE DRIVEN FROM HERE. Normally each zone's repeat timer runs
// from the moment ITS OWN button was pressed, so eight people pressing at
// slightly different times leaves the zones scattered across the second. For
// the payoff they have to land together, so during the poof the state machine
// writes fireState[] for all eight directly and the per-button pulse table is
// bypassed entirely. One timestamp, one burst.
//
// THE LOCKOUT COVERS ALL NINE OUTPUTS, not just FIRE9. While it is in effect
// no button does anything: pressing button 3 will not fire zone 3.
//
// INVARIANT: BIG_POOF_ZONE_MS <= BIG_POOF_DURATION_MS. The zones finish first,
// leaving FIRE9 alone for the tail of the poof.
//
// The poof is a fixed-length SHOT, not a hold: releasing the buttons partway
// does not cut it short. A known 2 s burst every time is more predictable than
// a variable one, and it cannot be extended by holding.
//
// Re-arming requires the all-8 combo to be RELEASED after the cooldown ends.
// Without that, a group leaning on the buttons would get a poof every 32 s
// forever with no deliberate action.
const unsigned long BIG_POOF_DURATION_MS = 1500;   // FIRE9
const unsigned long BIG_POOF_ZONE_MS     = 1000;   // FIRE1..FIRE8, in sync
const unsigned long BIG_POOF_COOLDOWN_MS = 30000;

// Backstop only. Normal pulse logic should return outputs to idle long
// before this. Also the Head Poof cutoff.
const unsigned long OUTPUT_CUTOFF_MS = 10000;

const unsigned long OLED_UPDATE_INTERVAL_MS = 250;
const unsigned long SERIAL_STATUS_INTERVAL_MS = 5000;

// FIRE indicator hold. Without this the OLED samples an instantaneous pin
// state: a 100 ms pulse every 1000 ms, sampled every 250 ms, is caught about
// 10% of the time — which is why the display used to read "FIRE OFF" while
// the poofers were visibly working. Latch the indicator on for this long
// after any fire activity so it reflects what a human sees.
const unsigned long FIRE_INDICATOR_HOLD_MS = 600;

// false = keep FIRE GPIOs idle HIGH; serial and OLED still show intent.
// true  = drive the live active-LOW FIRE outputs.
const bool FIRE_OUTPUTS_ENABLED = true;

// true  = ESP32 internal pull-downs, for bench work with nothing wired.
// false = live hardware with external 10k pull-downs at the button end.
const bool USE_INTERNAL_PULLDOWNS = false;

// =============================================================================
// BOOT SAFETY INTERLOCK
// =============================================================================
//
// If any button reads HIGH when we start, we do NOT arm. All FIRE outputs are
// held idle until every input has been observed LOW at least once.
//
// This covers a wedged button, a shorted button wire, and noise on a floating
// input at power-up. Any of those would otherwise mean fire the moment the
// board comes alive, with nobody expecting it.
//
// Once armed, it stays armed until reset.
const bool ENABLE_BOOT_INTERLOCK = true;

bool systemArmed = false;
bool buttonSeenLow[NUM_BUTTONS] = { false };

// =============================================================================
// PIN MAP  (ESP32-S3-DevKitC-1-N8R8)
// =============================================================================
//
// Button pins are the SAME GPIO numbers Eclair uses. One wire per button
// reaches both boards on the identically-numbered pin. Keep this invariant.
//
// The OLED is back on GPIO1/GPIO2 — those were LED lanes in the single-board
// build and are free again now that LEDs live on Eclair.

const int BUTTON_PINS[NUM_BUTTONS] = {
  4,  // Button 1
  5,  // Button 2
  6,  // Button 3
  7,  // Button 4
  15, // Button 5
  16, // Button 6
  17, // Button 7
  18  // Button 8
};

const int FIRE_PINS[NUM_FIRE_OUTPUTS] = {
  8,  // FIRE1
  9,  // FIRE2
  10, // FIRE3
  11, // FIRE4
  12, // FIRE5
  13, // FIRE6
  14, // FIRE7
  21, // FIRE8
  47  // FIRE9 / Head Poof
};

const int FIRE_IDLE_LEVEL = HIGH;
const int FIRE_TRIGGER_LEVEL = LOW;

// --- OLED ---
const int OLED_WIDTH = 128;
const int OLED_HEIGHT = 64;
const int OLED_RESET_PIN = -1;
const int OLED_I2C_ADDRESS = 0x3C;
const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 2;

#if ENABLE_OLED_HARDWARE
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
#endif

// =============================================================================
// STATE
// =============================================================================

bool rawButtonState[NUM_BUTTONS] = { false };
bool debouncedButtonState[NUM_BUTTONS] = { false };
bool lastRawButtonState[NUM_BUTTONS] = { false };
bool buttonPressEvent[NUM_BUTTONS] = { false };
unsigned long lastDebounceChangeMs[NUM_BUTTONS] = { 0 };
unsigned long buttonTriggerCount[NUM_BUTTONS] = { 0 };
unsigned long buttonGlitchCount[NUM_BUTTONS] = { 0 };

bool fireState[NUM_FIRE_OUTPUTS] = { false };
bool firePulseActive[NUM_FIRE_OUTPUTS] = { false };
unsigned long firePulseStartMs[NUM_FIRE_OUTPUTS] = { 0 };
unsigned long firePulseDurationMs[NUM_FIRE_OUTPUTS] = { 0 };
unsigned long lastFireRepeatMs[NUM_BUTTONS] = { 0 };
unsigned long fireFireCount[NUM_FIRE_OUTPUTS] = { 0 };

// FIRE9 is driven EXCLUSIVELY by this state machine. Nothing else may write
// fireState[FIRE9_INDEX]. The previous code had two paths both driving it -
// an all-8 pulse and a B1+B8 hold - which fought each other through the pulse
// table and dropped the output for a frame partway through.
enum BigPoofState {
  BIG_POOF_IDLE,
  BIG_POOF_FIRING,
  BIG_POOF_COOLDOWN
};

BigPoofState bigPoofState = BIG_POOF_IDLE;
unsigned long bigPoofStateMs = 0;
bool bigPoofArmed = true;

// Latched indicator — see FIRE_INDICATOR_HOLD_MS above.
unsigned long lastFireActivityMs = 0;
int lastFireIndex = -1;

unsigned long lastOledUpdateMs = 0;
unsigned long lastSerialStatusMs = 0;
bool oledReady = false;
String serialCommandBuffer = "";

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1500);

  setupPins();
  setupOled();

  Serial.println();
  Serial.println("=====================================");
  Serial.println(" TARDI - FIRE CONTROLLER");
  Serial.println(" no WiFi, no LEDs on this board");
  Serial.println("=====================================");
  Serial.printf("FIRE outputs : %s\n", FIRE_OUTPUTS_ENABLED ? "ENABLED" : "disabled");
  Serial.printf("pulldowns    : %s\n", USE_INTERNAL_PULLDOWNS ? "internal" : "external 10k");
  Serial.printf("interlock    : %s\n", ENABLE_BOOT_INTERLOCK ? "ON" : "off");
  Serial.println();
  printCommandHelp();
}

void setupPins() {
  // FIRE pins driven to idle BEFORE being made outputs, so there is no window
  // where the pin is an output at an undefined level.
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    digitalWrite(FIRE_PINS[i], FIRE_IDLE_LEVEL);
    pinMode(FIRE_PINS[i], OUTPUT);
    digitalWrite(FIRE_PINS[i], FIRE_IDLE_LEVEL);
  }

  // Buttons are active-HIGH. Never PULLUP here.
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], USE_INTERNAL_PULLDOWNS ? INPUT_PULLDOWN : INPUT);
  }

  if (!ENABLE_BOOT_INTERLOCK) systemArmed = true;
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  unsigned long now = millis();

  processSerialCommands();
  readButtons();
  updateButtonDebounce(now);
  updateArmingInterlock();

  if (systemArmed) {
    // ORDER MATTERS. The big poof runs FIRST so poofersLockedOut() reflects
    // this frame, not the last one.
    updateBigPoofLogic(now);

    if (bigPoofState == BIG_POOF_FIRING) {
      // The state machine owns every output during the poof. Do NOT call
      // suppressZonePoofers() here - it would wipe the synced zone burst that
      // updateBigPoofLogic() just set.
    } else if (bigPoofState == BIG_POOF_COOLDOWN) {
      suppressZonePoofers();
    } else {
      updateNormalFireLogic(now);
      updateFirePulseStates(now);
    }
  } else {
    clearFireStates();
  }

  updateFireOutputs();
  updateFireIndicator(now);
  updateOled(now);
  printSerialStatusIfDue(now);
}

// =============================================================================
// BUTTONS
// =============================================================================

void readButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    rawButtonState[i] = (digitalRead(BUTTON_PINS[i]) == HIGH);
  }
}

// A level must hold steady for DEBOUNCE_MS before we act on it. Shorter
// changes are counted as glitches and discarded. The glitch counter is in the
// status output — on a fire board, a climbing glitch count is a warning worth
// acting on, because the failure mode is unintended fire.
void updateButtonDebounce(unsigned long now) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttonPressEvent[i] = false;

    if (rawButtonState[i] != lastRawButtonState[i]) {
      if (lastRawButtonState[i] != debouncedButtonState[i]) {
        buttonGlitchCount[i]++;
      }
      lastRawButtonState[i] = rawButtonState[i];
      lastDebounceChangeMs[i] = now;
      continue;
    }

    if ((now - lastDebounceChangeMs[i]) < DEBOUNCE_MS) continue;
    if (rawButtonState[i] == debouncedButtonState[i]) continue;

    bool risingEdge = (rawButtonState[i] && !debouncedButtonState[i]);
    debouncedButtonState[i] = rawButtonState[i];
    if (risingEdge) {
      buttonPressEvent[i] = true;
      buttonTriggerCount[i]++;
    }
  }
}

bool areAllButtonsPressed() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!debouncedButtonState[i]) return false;
  }
  return true;
}

// =============================================================================
// ARMING INTERLOCK
// =============================================================================

void updateArmingInterlock() {
  if (systemArmed) return;

  // BOTH raw and debounced must read low.
  //
  // Checking only the debounced state was a latent bug that made this
  // interlock useless: debouncedButtonState[] initialises to false and takes
  // DEBOUNCE_MS to catch up, so on the very FIRST loop iteration every button
  // looked low, all eight were marked seen-low, and the board armed itself
  // before debounce had resolved. Holding buttons at power-up armed instantly
  // and then fired - exactly what this is supposed to prevent.
  //
  // rawButtonState[] is sampled directly from the pins in readButtons(), so it
  // is true from the first pass if a button is genuinely held.
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!rawButtonState[i] && !debouncedButtonState[i]) buttonSeenLow[i] = true;
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!buttonSeenLow[i]) return;      // still waiting on this one
  }

  systemArmed = true;
  Serial.println("*** ARMED - all inputs seen low, fire outputs live ***");
}

int countUnreleasedButtons() {
  int n = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!buttonSeenLow[i]) n++;
  }
  return n;
}

// =============================================================================
// FIRE LOGIC
// =============================================================================

void clearFireStates() {
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    fireState[i] = false;
    firePulseActive[i] = false;
  }
  // Held un-armed, so the interlock cannot be escaped by holding all 8 while
  // the board comes up.
  bigPoofState = BIG_POOF_IDLE;
  bigPoofArmed = false;
}

void startFirePulseForDuration(int fireIndex, unsigned long pulseDurationMs) {
  if (fireIndex < 0 || fireIndex >= NUM_FIRE_OUTPUTS) return;
  firePulseActive[fireIndex] = true;
  firePulseStartMs[fireIndex] = millis();
  firePulseDurationMs[fireIndex] = pulseDurationMs;
  fireFireCount[fireIndex]++;
  lastFireActivityMs = millis();
  lastFireIndex = fireIndex;
}

void startFirePulse(int fireIndex) {
  startFirePulseForDuration(fireIndex, NORMAL_FIRE_PULSE_MS);
}

// Buttons 1-8 each start a repeating FIRE1-FIRE8 pulse while held.
void updateNormalFireLogic(unsigned long now) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonPressEvent[i]) {
      startFirePulse(i);
      lastFireRepeatMs[i] = now;
      continue;
    }

    if (!debouncedButtonState[i]) {
      lastFireRepeatMs[i] = 0;
      continue;
    }

    if ((now - lastFireRepeatMs[i]) >= FIRE_REPEAT_INTERVAL_MS) {
      startFirePulse(i);
      lastFireRepeatMs[i] = now;
    }
  }
}


// All 8 buttons held = the big poof. See the constants at the top of the file.
//
// This replaced two separate paths: a 500 ms burst across every output on
// all-8, and a sustained FIRE9 on Button 1 + Button 8. Both drove FIRE9, they
// interacted through the shared pulse table, and all-8 satisfied both at once.
// B1+B8 now has no special meaning on this board.
bool isBigPoofRequested() {
  return areAllButtonsPressed();
}

// True during the poof AND the cooldown. While true, NO poofer may fire.
bool poofersLockedOut() {
  return bigPoofState == BIG_POOF_FIRING || bigPoofState == BIG_POOF_COOLDOWN;
}

// Cancel anything in flight and stop the repeat timers. Called every loop
// while locked out, so a pulse cannot survive into the lockout and a held
// button cannot accumulate a queued repeat.
//
// Zeroing lastFireRepeatMs also means a still-held button resumes promptly
// when the lockout lifts, rather than waiting out a stale interval.
void suppressZonePoofers() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    firePulseActive[i] = false;
    firePulseDurationMs[i] = 0;
    firePulseStartMs[i] = 0;
    fireState[i] = false;
    lastFireRepeatMs[i] = 0;
  }
}

bool bigPoofIsCoolingDown() {
  return bigPoofState == BIG_POOF_COOLDOWN;
}

unsigned long bigPoofCooldownRemainingMs(unsigned long now) {
  if (bigPoofState != BIG_POOF_COOLDOWN) return 0;
  unsigned long elapsed = now - bigPoofStateMs;
  return (elapsed >= BIG_POOF_COOLDOWN_MS) ? 0 : (BIG_POOF_COOLDOWN_MS - elapsed);
}

void updateBigPoofLogic(unsigned long now) {
  bool allHeld = isBigPoofRequested();

  // Re-arm only once the combo has been released AND we are back in IDLE.
  //
  // The IDLE check matters. Without it, a release at any point DURING the
  // cooldown counted as re-arming, so a group that held all eight, slipped
  // once, and kept holding would get another poof the instant the 30 s
  // expired - with nobody having done anything deliberate. Requiring the
  // release to happen after the cooldown has finished means every poof is an
  // intentional act.
  if (!allHeld && bigPoofState == BIG_POOF_IDLE) bigPoofArmed = true;

  switch (bigPoofState) {
    case BIG_POOF_IDLE:
      fireState[FIRE9_INDEX] = false;
      if (allHeld && bigPoofArmed) {
        bigPoofState = BIG_POOF_FIRING;
        bigPoofStateMs = now;
        bigPoofArmed = false;
        fireFireCount[FIRE9_INDEX]++;

        // Clear the per-button pulse table so nothing in flight competes,
        // then light all eight zones from THIS timestamp. Counted here so the
        // per-output tallies still reflect reality.
        suppressZonePoofers();
        for (int i = 0; i < NUM_BUTTONS; i++) {
          fireState[i] = true;
          fireFireCount[i]++;
        }
        lastFireActivityMs = now;
        lastFireIndex = FIRE9_INDEX;
        Serial.println("BIG POOF: firing - all zones in sync");
      }
      break;

    case BIG_POOF_FIRING: {
      // All time comparisons are (now - start) >= duration, which is safe
      // across the millis() rollover at ~49 days. Never (now >= start + d).
      unsigned long elapsed = now - bigPoofStateMs;

      // Zones: all eight together, off as one. Driven from the poof's clock,
      // not from each button's own repeat timer.
      bool zonesOn = (elapsed < BIG_POOF_ZONE_MS);
      for (int i = 0; i < NUM_BUTTONS; i++) {
        fireState[i] = zonesOn;
      }

      if (elapsed >= BIG_POOF_DURATION_MS) {
        for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) fireState[i] = false;
        bigPoofState = BIG_POOF_COOLDOWN;
        bigPoofStateMs = now;
        Serial.printf("BIG POOF: done, %lu s lockout\n",
                      BIG_POOF_COOLDOWN_MS / 1000UL);
      } else {
        fireState[FIRE9_INDEX] = true;
        lastFireActivityMs = now;
        lastFireIndex = FIRE9_INDEX;
      }
      break;
    }

    case BIG_POOF_COOLDOWN:
      fireState[FIRE9_INDEX] = false;
      if ((now - bigPoofStateMs) >= BIG_POOF_COOLDOWN_MS) {
        bigPoofState = BIG_POOF_IDLE;
        Serial.println("BIG POOF: ready");
      }
      break;
  }
}

unsigned long getPulseDurationMs(int fireIndex) {
  if (fireIndex >= 0 && fireIndex < NUM_FIRE_OUTPUTS &&
      firePulseDurationMs[fireIndex] > 0) {
    return firePulseDurationMs[fireIndex];
  }
  return NORMAL_FIRE_PULSE_MS;
}

// Deliberately 0..NUM_BUTTONS-1, NOT NUM_FIRE_OUTPUTS. FIRE9 is owned by the
// big-poof state machine and must never be written here - that overlap is what
// made the old code drop FIRE9 for a frame partway through a head poof.
void updateFirePulseStates(unsigned long now) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!firePulseActive[i]) continue;

    unsigned long pulseDuration = getPulseDurationMs(i);
    unsigned long elapsedMs = now - firePulseStartMs[i];

    if (elapsedMs >= pulseDuration || elapsedMs >= OUTPUT_CUTOFF_MS) {
      firePulseActive[i] = false;
      firePulseStartMs[i] = 0;
      firePulseDurationMs[i] = 0;
      fireState[i] = false;
    } else {
      fireState[i] = true;
      lastFireActivityMs = now;
      lastFireIndex = i;
    }
  }
}

void updateFireOutputs() {
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    bool drive = FIRE_OUTPUTS_ENABLED && systemArmed && fireState[i];
    digitalWrite(FIRE_PINS[i], drive ? FIRE_TRIGGER_LEVEL : FIRE_IDLE_LEVEL);
  }
}

// =============================================================================
// FIRE INDICATOR (latched)
// =============================================================================

bool anyFireStateNow() {
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    if (fireState[i]) return true;
  }
  return false;
}

bool fireIndicatorActive = false;

void updateFireIndicator(unsigned long now) {
  if (anyFireStateNow()) {
    lastFireActivityMs = now;
    fireIndicatorActive = true;
    return;
  }
  fireIndicatorActive = (lastFireActivityMs != 0) &&
                        ((now - lastFireActivityMs) < FIRE_INDICATOR_HOLD_MS);
}

bool hasAnyActiveInput() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (debouncedButtonState[i]) return true;
  }
  return false;
}

String getInputLabel() {
  String s = "";
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (debouncedButtonState[i]) {
      if (s.length() > 0) s += ",";
      s += String(i + 1);
    }
  }
  return s.length() > 0 ? s : "-";
}

String getFireLabel() {
  if (isBigPoofRequested() && fireState[FIRE9_INDEX]) return "HEAD POOF";
  String s = "";
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    if (fireState[i]) {
      if (s.length() > 0) s += ",";
      s += String(i + 1);
    }
  }
  if (s.length() > 0) return s;
  if (fireIndicatorActive && lastFireIndex >= 0) return String(lastFireIndex + 1);
  return "";
}

// =============================================================================
// OLED
// =============================================================================

void setupOled() {
#if ENABLE_OLED_HARDWARE
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
  if (!oledReady) {
    Serial.println("OLED setup failed.");
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("TARDI FIRE");
  display.println();
  display.println("booting...");
  display.display();
#else
  oledReady = false;
#endif
}

void drawOledLine(int line, const String &text) {
#if ENABLE_OLED_HARDWARE
  display.setCursor(0, line * 10);
  display.println(text);
#endif
}

void updateOled(unsigned long now) {
#if ENABLE_OLED_HARDWARE
  if (!oledReady) return;
  if ((now - lastOledUpdateMs) < OLED_UPDATE_INTERVAL_MS) return;
  lastOledUpdateMs = now;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (!systemArmed) {
    // Blink so a not-armed board is unmistakable across a dark playa.
    bool blink = ((now / 400) % 2) == 0;
    if (blink) {
      display.fillRect(0, 0, OLED_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    drawOledLine(0, "NOT ARMED");
    display.setTextColor(SSD1306_WHITE);
    drawOledLine(1, "release buttons");
    drawOledLine(2, "held: " + String(countUnreleasedButtons()));
    drawOledLine(4, "fire outputs OFF");
    display.display();
    return;
  }

  drawOledLine(0, FIRE_OUTPUTS_ENABLED ? "LIVE" : "SIMULATED");
  drawOledLine(1, fireIndicatorActive ? "FIRING" :
                  (hasAnyActiveInput() ? "PULSE COMPLETE" : "READY"));
  drawOledLine(2, "Input: " + getInputLabel());

  // Latched, so a 100 ms pulse sampled every 250 ms is still visible.
  String fire = getFireLabel();
  drawOledLine(3, fire.length() > 0 ? ("FIRE: " + fire) : "FIRE: OFF");

  unsigned long glitches = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) glitches += buttonGlitchCount[i];
  if (glitches > 0) {
    drawOledLine(5, "noise: " + String(glitches));
  }

  display.display();
#endif
}

// =============================================================================
// SERIAL
// =============================================================================

void printCommandHelp() {
  Serial.println("commands:");
  Serial.println("  status      full status now");
  Serial.println("  arm         override the boot interlock (use with care)");
  Serial.println("  counts      per-button trigger and glitch counts");
  Serial.println("  poof        big poof state and cooldown remaining");
  Serial.println("  help");
}

void printSerialStatusIfDue(unsigned long now) {
  if ((now - lastSerialStatusMs) < SERIAL_STATUS_INTERVAL_MS) return;
  lastSerialStatusMs = now;
  printStatus(now);
}

void printStatus(unsigned long now) {
  Serial.println();
  Serial.println("=== TARDI - FIRE CONTROLLER =========================");

  if (!systemArmed) {
    Serial.printf("*** NOT ARMED *** %d input(s) never seen low.\n",
                  countUnreleasedButtons());
    Serial.println("    All FIRE outputs held idle. Release every button,");
    Serial.println("    or check for a stuck switch / noisy wire.");
    Serial.println();
  }

  Serial.println("btn  pin  state  trig  glitch    fire  gpio  out   count");
  for (int i = 0; i < NUM_BUTTONS; i++) {
    Serial.printf("  %d  %3d  %-5s %5lu  %6lu    FIRE%d %4d  %-4s %5lu\n",
                  i + 1, BUTTON_PINS[i],
                  debouncedButtonState[i] ? "HI" : "lo",
                  buttonTriggerCount[i], buttonGlitchCount[i],
                  i + 1, FIRE_PINS[i],
                  fireState[i] ? "FIRE" : "idle",
                  fireFireCount[i]);
  }
  Serial.printf("  -    -  -      -       -        FIRE9 %4d  %-4s %5lu  (head poof)\n",
                FIRE_PINS[FIRE9_INDEX],
                fireState[FIRE9_INDEX] ? "FIRE" : "idle",
                fireFireCount[FIRE9_INDEX]);

  unsigned long glitches = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) glitches += buttonGlitchCount[i];

  Serial.println();
  Serial.printf("armed  : %s\n", systemArmed ? "YES" : "NO");
  Serial.printf("outputs: %s (active-LOW, idle HIGH)\n",
                FIRE_OUTPUTS_ENABLED ? "ENABLED" : "disabled");
  const char *bpState = bigPoofState == BIG_POOF_FIRING   ? "FIRING"
                      : bigPoofState == BIG_POOF_COOLDOWN ? "COOLDOWN"
                      : (bigPoofArmed ? "ready" : "waiting for release");
  Serial.printf("bigpoof: all-8 %s   state %s", 
                isBigPoofRequested() ? "HELD" : "no", bpState);
  if (bigPoofState == BIG_POOF_COOLDOWN) {
    Serial.printf("  %lu s left", (bigPoofCooldownRemainingMs(now) + 999UL) / 1000UL);
  }
  Serial.printf("   fired %lu times\n", fireFireCount[FIRE9_INDEX]);
  Serial.printf("pulses : zone %lu ms every %lu ms\n",
                NORMAL_FIRE_PULSE_MS, FIRE_REPEAT_INTERVAL_MS);
  Serial.printf("bigpoof: zones %lu ms in sync, FIRE9 %lu ms, then %lu s lockout\n",
                BIG_POOF_ZONE_MS, BIG_POOF_DURATION_MS,
                BIG_POOF_COOLDOWN_MS / 1000UL);
  if (poofersLockedOut()) {
    Serial.println("         *** ALL POOFERS LOCKED OUT ***");
  }
  Serial.printf("buttons: debounce %lu ms, pulldown %s, %lu glitches rejected\n",
                DEBOUNCE_MS, USE_INTERNAL_PULLDOWNS ? "internal" : "external",
                glitches);
  if (glitches > 0) {
    Serial.println("         GLITCHES ON A FIRE BOARD ARE WORTH INVESTIGATING.");
    Serial.println("         Fit 1k series + 100nF to GND at each button input.");
  }
  Serial.printf("oled   : %s\n", oledReady ? "ready" : "not present");
  Serial.println("=====================================================");
}

void processSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialCommandBuffer.trim();
      if (serialCommandBuffer.length() > 0) handleSerialCommand(serialCommandBuffer);
      serialCommandBuffer = "";
    } else {
      serialCommandBuffer += c;
      if (serialCommandBuffer.length() > 64) serialCommandBuffer = "";
    }
  }
}

void handleSerialCommand(String cmd) {
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") { printCommandHelp(); return; }
  if (cmd == "status") { printStatus(millis()); return; }

  if (cmd == "arm") {
    if (systemArmed) {
      Serial.println("already armed");
    } else {
      Serial.println("*** interlock overridden from serial ***");
      Serial.printf("    %d input(s) were still high.\n", countUnreleasedButtons());
      for (int i = 0; i < NUM_BUTTONS; i++) buttonSeenLow[i] = true;
      systemArmed = true;
    }
    return;
  }

  if (cmd == "poof") {
    unsigned long now = millis();
    Serial.printf("big poof : %s\n",
                  bigPoofState == BIG_POOF_FIRING   ? "FIRING"
                : bigPoofState == BIG_POOF_COOLDOWN ? "COOLDOWN"
                : (bigPoofArmed ? "ready" : "waiting for all-8 to be released"));
    if (bigPoofState == BIG_POOF_COOLDOWN) {
      Serial.printf("cooldown : %lu ms remaining of %lu ms\n",
                    bigPoofCooldownRemainingMs(now), BIG_POOF_COOLDOWN_MS);
    }
    Serial.printf("trigger  : all %d buttons held together\n", NUM_BUTTONS);
    Serial.printf("zones    : %lu ms, all eight in sync\n", BIG_POOF_ZONE_MS);
    Serial.printf("FIRE9    : %lu ms, then %lu s lockout on ALL poofers\n",
                  BIG_POOF_DURATION_MS, BIG_POOF_COOLDOWN_MS / 1000UL);
    Serial.printf("locked   : %s\n", poofersLockedOut() ? "YES - no poofer will fire" : "no");
    Serial.printf("fired    : %lu times since boot\n", fireFireCount[FIRE9_INDEX]);
    return;
  }

  if (cmd == "counts") {
    for (int i = 0; i < NUM_BUTTONS; i++) {
      Serial.printf("button %d: %lu triggers, %lu glitches\n",
                    i + 1, buttonTriggerCount[i], buttonGlitchCount[i]);
    }
    return;
  }

  Serial.print("unknown command: ");
  Serial.println(cmd);
}
