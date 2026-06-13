#include <Arduino.h>

#include "led_engine.h"
#include "led_expander_output.h"
#include "led_layout.h"
#include "led_state.h"

// ==================================================
// Tardi Controller
// ESP32-S3 controller for Resilience tardigrade build
// ==================================================
//
// Firmware scaffold for:
// - 8 active-HIGH button inputs
// - 9 active-LOW FIRE outputs named FIRE1-FIRE9
// - FIRE outputs idle HIGH
// - FIRE outputs trigger LOW
// - FIRE1-FIRE8 normal one-shot fire outputs
// - FIRE9 / Big Poof active only while Button 1 + Button 8 are held
// - Serial diagnostics
// - OLED diagnostics
//
// Full baseline details live in docs/current_baseline.md.
// Pin mapping rules live in docs/pin_mapping.md.

// ==================================================
// OLED HARDWARE SWITCH
// ==================================================
//
// 0 = OLED code is compiled out.
//     Use this before the OLED display and libraries are installed.
//
// 1 = OLED code is compiled in.
//     Requires these Arduino libraries:
//     - Adafruit SSD1306
//     - Adafruit GFX Library
//
// The expected OLED is:
// - 0.96 inch
// - 128x64
// - SSD1306 driver
// - I2C / IIC
// - likely address 0x3C

#define ENABLE_OLED_HARDWARE 1

#if ENABLE_OLED_HARDWARE
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

// ==================================================
// CONFIG
// ==================================================

const int NUM_BUTTONS = 8;
const int NUM_FIRE_OUTPUTS = 9;

const int FIRE9_INDEX = 8;

const unsigned long DEBOUNCE_MS = 30;

// FIRE1-FIRE8: 500 ms one-shot pulse.
// FIRE9 / Big Poof follows the Button 1 + Button 8 hold state.
const unsigned long NORMAL_FIRE_PULSE_MS = 500;

// Backup guard only.
// Normal pulse behavior should return outputs HIGH after 500 ms.
const unsigned long OUTPUT_CUTOFF_MS = 10000;

// Prevents Serial flooding while keeping the loop non-blocking.
const unsigned long SERIAL_DEBUG_INTERVAL_MS = 100;

// OLED refresh should be slower than the main loop.
// This keeps display updates lightweight.
const unsigned long OLED_UPDATE_INTERVAL_MS = 250;

// India-safe simulated Output Expander diagnostics over USB Serial only.
// This does not start Serial1 or send Pixelblaze Output Expander frames.
const bool ENABLE_EXPANDER_SIM_SERIAL_DIAGNOSTICS = true;
const unsigned long EXPANDER_SIM_SERIAL_DIAGNOSTIC_INTERVAL_MS = 5000;

// false = keep FIRE GPIOs idle HIGH; Serial/OLED still show requested state
// true  = allow FIRE GPIOs to drive the current bench LED outputs
// Set false before connecting real relay/poofer hardware unless the hardware has been confirmed.
const bool FIRE_OUTPUTS_ENABLED = true;

// ==================================================
// INPUT PULLDOWN MODE
// ==================================================
//
// true  = India bench testing with ESP32 internal pull-downs
// false = California final hardware with external 10kΩ pull-downs
//
// Logical behavior is unchanged:
// LOW  = inactive
// HIGH = active
//
// Preferred button wiring:
// ESP32 3.3V -> button panel -> button return wire -> ESP32 GPIO input
const bool USE_INTERNAL_PULLDOWNS = true;

// ==================================================
// OLED CONFIG
// ==================================================
//
// Current expected module:
// - SSD1306
// - 128x64
// - I2C
// - 4 pins: GND, VCC, SCL, SDA
// - likely address 0x3C
//
// OLED I2C pins are assigned from the current bench-tested map.
// OLED SDA = GPIO1
// OLED SCL = GPIO2

const int OLED_WIDTH = 128;
const int OLED_HEIGHT = 64;
const int OLED_RESET_PIN = -1;
const int OLED_I2C_ADDRESS = 0x3C;

const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 2;

#if ENABLE_OLED_HARDWARE
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
#endif

// ==================================================
// PIN CONFIG
// ==================================================
//
// Current board:
// ESP32-S3-DevKitC-1-N8R8
//
// Temporary bench-test pin map only.
// Not final California wiring.
//
// Keep all raw GPIO numbers in this section.
//
// Avoid/reserve for now:
// - GPIO0: boot/download-mode related; read HIGH during bench input test
// - GPIO48: likely onboard RGB/status LED related
// - TX/RX: serial/programming related
// - GPIO1/GPIO2: OLED SDA/SCL
//

// OLED is assigned to GPIO1/GPIO2.
// Future LED Output Expander UART pins are not assigned yet.
// First candidate pool for future LED UART/diagnostics: GPIO39, GPIO40, GPIO41.

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
  47  // FIRE9 / big poof
};

const int FIRE_IDLE_LEVEL = HIGH;
const int FIRE_TRIGGER_LEVEL = LOW;

// ==================================================
// STATE
// ==================================================

bool rawButtonState[NUM_BUTTONS] = { false };
bool debouncedButtonState[NUM_BUTTONS] = { false };
bool lastRawButtonState[NUM_BUTTONS] = { false };

bool buttonPressEvent[NUM_BUTTONS] = { false };

unsigned long lastDebounceChangeMs[NUM_BUTTONS] = { 0 };
unsigned long buttonPressedStartMs[NUM_BUTTONS] = { 0 };

bool fireState[NUM_FIRE_OUTPUTS] = { false };
bool firePulseActive[NUM_FIRE_OUTPUTS] = { false };
unsigned long firePulseStartMs[NUM_FIRE_OUTPUTS] = { 0 };

unsigned long bigPoofStartMs = 0;

unsigned long lastSerialDebugMs = 0;
unsigned long lastOledUpdateMs = 0;
unsigned long lastExpanderSimDiagnosticMs = 0;

bool oledReady = false;

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);
  setupPins();
  delay(500);

  Serial.println();
  Serial.println("Tardi Controller starting...");
  Serial.println(FIRE_OUTPUTS_ENABLED ? "FIRE GPIO outputs: ENABLED" : "FIRE GPIO outputs: DISABLED");

  Serial.print("Input pulldown mode: ");
  Serial.println(USE_INTERNAL_PULLDOWNS ? "INTERNAL / BENCH" : "EXTERNAL / FINAL");

  Serial.print("OLED hardware compiled: ");
  Serial.println(ENABLE_OLED_HARDWARE ? "YES" : "NO");

  ledStateBegin();
  ledEngineBegin();
  ledExpanderOutputBegin();
  setupOled();
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  readButtons();
  updateButtonDebounce();
  updateInteractionLogic();
  updateLedTriggers();
  updateLedOutputs();
  updateFireOutputs();
  printSerialDebugIfDue();
  updateOled();

  // No blocking delay here.
}

// ==================================================
// PIN SETUP
// ==================================================

void setupPins() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (USE_INTERNAL_PULLDOWNS) {
      pinMode(BUTTON_PINS[i], INPUT_PULLDOWN);
    } else {
      pinMode(BUTTON_PINS[i], INPUT);
    }
  }

  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    digitalWrite(FIRE_PINS[i], FIRE_IDLE_LEVEL);
    pinMode(FIRE_PINS[i], OUTPUT);
    digitalWrite(FIRE_PINS[i], FIRE_IDLE_LEVEL);
  }
}

// ==================================================
// BUTTON READING
// ==================================================

void readButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    rawButtonState[i] = digitalRead(BUTTON_PINS[i]) == HIGH;
  }
}

void updateButtonDebounce() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttonPressEvent[i] = false;

    if (rawButtonState[i] != lastRawButtonState[i]) {
      lastDebounceChangeMs[i] = now;
      lastRawButtonState[i] = rawButtonState[i];
    }

    if ((now - lastDebounceChangeMs[i]) >= DEBOUNCE_MS) {
      if (debouncedButtonState[i] != rawButtonState[i]) {
        bool previousDebouncedState = debouncedButtonState[i];

        debouncedButtonState[i] = rawButtonState[i];

        if (debouncedButtonState[i]) {
          buttonPressedStartMs[i] = now;

          if (!previousDebouncedState) {
            buttonPressEvent[i] = true;
          }
        } else {
          buttonPressedStartMs[i] = 0;
        }
      }
    }
  }
}

// ==================================================
// INTERACTION LOGIC
// ==================================================

void updateInteractionLogic() {
  updateInteractionState();
  clearFireStates();
  updateNormalFireLogic();
  updateBigPoofLogic();
  updateFirePulseStates();
}

void updateInteractionState() {
  // Future shared interaction state can be built here.
  // Keep this separate from FIRE-only behavior.
}

void clearFireStates() {
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    fireState[i] = false;
  }
}

void startFirePulse(int fireIndex) {
  if (fireIndex < 0 || fireIndex >= NUM_FIRE_OUTPUTS) {
    return;
  }

  firePulseActive[fireIndex] = true;
  firePulseStartMs[fireIndex] = millis();
}

void updateNormalFireLogic() {
  // Button 1-8 each start one matching 500 ms FIRE1-FIRE8 pulse.
  // Holding a button does not keep FIRE active.

  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonPressEvent[i]) {
      startFirePulse(i);
    }
  }
}

// ==================================================
// BIG POOF LOGIC
// ==================================================
//
// Current trigger:
// Button 1 + Button 8 pressed together -> FIRE9
//
// Current behavior:
// FIRE9 is active only while Button 1 and Button 8 are both held.
// Releasing either input turns FIRE9 off.
// A 10 second cutoff still prevents a stuck Big Poof output.

bool isBigPoofRequested() {
  return debouncedButtonState[0] && debouncedButtonState[7];
}

void updateBigPoofLogic() {
  bool bigPoofRequested = isBigPoofRequested();

  if (!bigPoofRequested) {
    bigPoofStartMs = 0;
    fireState[FIRE9_INDEX] = false;
    return;
  }

  unsigned long now = millis();

  if (bigPoofStartMs == 0) {
    bigPoofStartMs = now;
  }

  if ((now - bigPoofStartMs) < OUTPUT_CUTOFF_MS) {
    fireState[FIRE9_INDEX] = true;
  } else {
    fireState[FIRE9_INDEX] = false;
  }
}

// ==================================================
// FIRE PULSE STATE
// ==================================================

void updateFirePulseStates() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    if (!firePulseActive[i]) {
      continue;
    }

    unsigned long pulseDuration = getPulseDurationMs(i);
    unsigned long elapsedMs = now - firePulseStartMs[i];

    if (elapsedMs >= pulseDuration || elapsedMs >= OUTPUT_CUTOFF_MS) {
      firePulseActive[i] = false;
      firePulseStartMs[i] = 0;
      fireState[i] = false;
    } else {
      fireState[i] = true;
    }
  }
}

unsigned long getPulseDurationMs(int fireIndex) {
  return NORMAL_FIRE_PULSE_MS;
}

// ==================================================
// LED HOOKS
// ==================================================

void updateLedTriggers() {
  unsigned long now = millis();

  if (buttonPressEvent[0]) {
    ledTriggerZone(LED_ZONE_Z1_MOUTH, now);
  }

  if (buttonPressEvent[1]) {
    ledTriggerZone(LED_ZONE_Z2_SHOULDER, now);
  }

  if (buttonPressEvent[2]) {
    ledTriggerZone(LED_ZONE_Z3_MIDBODY, now);
  }

  if (buttonPressEvent[3]) {
    ledTriggerZone(LED_ZONE_Z4_REAR, now);
  }

  if (buttonPressEvent[4]) {
    ledTriggerZone(LED_ZONE_Z5_FRONT_LEGS, now);
  }

  if (buttonPressEvent[5]) {
    ledTriggerZone(LED_ZONE_Z6_BACK_LEGS, now);
  }

  if (buttonPressEvent[6]) {
    ledTriggerZone(LED_ZONE_Z7_DIGESTIVE, now);
  }
}

void updateLedOutputs() {
  unsigned long now = millis();
  ledEngineUpdate(now);
  ledExpanderOutputUpdate(now);

  if (
    ENABLE_EXPANDER_SIM_SERIAL_DIAGNOSTICS
    && (now - lastExpanderSimDiagnosticMs) >= EXPANDER_SIM_SERIAL_DIAGNOSTIC_INTERVAL_MS
  ) {
    lastExpanderSimDiagnosticMs = now;
    ledExpanderOutputPrintSimFrameStats(now, Serial);
  }

  // Future direct LED output hook.
  // Direct ESP32 LED control is not part of the current fire-control scaffold.
}

// ==================================================
// FIRE OUTPUTS
// ==================================================

void updateFireOutputs() {
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    if (FIRE_OUTPUTS_ENABLED) {
      digitalWrite(FIRE_PINS[i], fireState[i] ? FIRE_TRIGGER_LEVEL : FIRE_IDLE_LEVEL);
    } else {
      digitalWrite(FIRE_PINS[i], FIRE_IDLE_LEVEL);
    }
  }
}

// ==================================================
// SERIAL DEBUG
// ==================================================

void printSerialDebugIfDue() {
  unsigned long now = millis();

  if ((now - lastSerialDebugMs) < SERIAL_DEBUG_INTERVAL_MS) {
    return;
  }

  lastSerialDebugMs = now;
  printSerialDebug();
}

void printSerialDebug() {
  Serial.print("Buttons: ");

  for (int i = 0; i < NUM_BUTTONS; i++) {
    Serial.print("B");
    Serial.print(i + 1);
    Serial.print("=");
    Serial.print(debouncedButtonState[i] ? "1" : "0");

    if (buttonPressEvent[i]) {
      Serial.print("P ");
    } else {
      Serial.print("  ");
    }
  }

  Serial.print(" | Fire: ");

  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    Serial.print("FIRE");
    Serial.print(i + 1);
    Serial.print("=");
    Serial.print(fireState[i] ? "1" : "0");

    if (firePulseActive[i]) {
      Serial.print("P ");
    } else {
      Serial.print("  ");
    }
  }

  Serial.print(" | BigPoof=");
  Serial.print(isBigPoofRequested() ? "1" : "0");

  Serial.print(" | Outputs=");
  Serial.print(FIRE_OUTPUTS_ENABLED ? "ON" : "OFF");

  Serial.print(" | PullDown=");
  Serial.print(USE_INTERNAL_PULLDOWNS ? "INTERNAL" : "EXTERNAL");

  Serial.print(" | OLED=");
  Serial.println(ENABLE_OLED_HARDWARE ? (oledReady ? "READY" : "ERROR") : "OFF");
}

// ==================================================
// OLED DISPLAY
// ==================================================

void setupOled() {
#if ENABLE_OLED_HARDWARE
  if (OLED_SDA_PIN >= 0 && OLED_SCL_PIN >= 0) {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  } else {
    Wire.begin();
  }

  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);

  if (!oledReady) {
    Serial.println("OLED setup failed.");
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(FIRE_OUTPUTS_ENABLED ? "GPIO OUT ON" : "GPIO OUT OFF");
  display.println();
  display.println("OLED READY");
  display.display();

  Serial.println("OLED setup complete.");
#else
  oledReady = false;
  Serial.println("OLED disabled at compile time.");
#endif
}

void updateOled() {
#if ENABLE_OLED_HARDWARE
  if (!oledReady) {
    return;
  }

  unsigned long now = millis();

  if ((now - lastOledUpdateMs) < OLED_UPDATE_INTERVAL_MS) {
    return;
  }

  lastOledUpdateMs = now;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  drawOledLine(0, FIRE_OUTPUTS_ENABLED ? "GPIO OUT ON" : "GPIO OUT OFF");
  drawOledLine(1, getOledStatusLabel());

  if (hasAnyActiveInput()) {
    drawOledLineWithValue(2, "Input:", getInputDisplayLabel());
  } else {
    drawOledLine(2, "Input: -");
  }

  if (hasAnyActiveFireOutput()) {
    drawOledLineWithValue(3, "Output:", getOutputDisplayLabel());
  } else {
    drawOledLine(3, "Output: OFF");
  }

  drawOledLineWithValue(4, "LED:", getLedDisplayLabel());

  if (FIRE_OUTPUTS_ENABLED) {
    drawOledLine(5, hasAnyActiveFireOutput() ? "GPIO output: ON" : "GPIO output: OFF");
  } else {
    drawOledLine(5, "GPIO output: OFF");
  }

  display.display();
#endif
}

#if ENABLE_OLED_HARDWARE
void drawOledLine(int lineNumber, const String &text) {
  const int lineHeight = 10;
  display.setCursor(0, lineNumber * lineHeight);
  display.println(text);
}

void drawOledLineWithValue(int lineNumber, const String &label, const String &value) {
  const int lineHeight = 10;
  display.setCursor(0, lineNumber * lineHeight);
  display.print(label);
  display.print(" ");
  display.println(value);
}

String getOledStatusLabel() {
  if (isBigPoofDisplayActive()) {
    return "BIG POOF";
  }

  if (hasAnyActiveFireOutput()) {
    return "FIRING";
  }

  if (hasAnyActiveInput()) {
    return "PULSE COMPLETE";
  }

  return "READY";
}

String getInputDisplayLabel() {
  if (!hasAnyActiveInput()) {
    return "-";
  }

  if (debouncedButtonState[0] && debouncedButtonState[7]) {
    return "1+8";
  }

  String label = "";

  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (debouncedButtonState[i]) {
      if (label.length() > 0) {
        label += "+";
      }

      label += String(i + 1);
    }
  }

  return label;
}

String getOutputDisplayLabel() {
  if (!hasAnyActiveFireOutput()) {
    return "OFF";
  }

  String label = "";

  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    if (fireState[i]) {
      if (label.length() > 0) {
        label += " ";
      }

      label += String(i + 1);
    }
  }

  return label;
}

String getLedDisplayLabel() {
  if (isBigPoofRequested() || fireState[FIRE9_INDEX]) {
    return "BIG";
  }

  if (hasAnyActiveInput()) {
    return getInputDisplayLabel();
  }

  return "-";
}

bool hasAnyActiveInput() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (debouncedButtonState[i]) {
      return true;
    }
  }

  return false;
}

bool hasAnyActiveFireOutput() {
  for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
    if (fireState[i]) {
      return true;
    }
  }

  return false;
}

bool isBigPoofDisplayActive() {
  return isBigPoofRequested() || fireState[FIRE9_INDEX];
}
#endif
