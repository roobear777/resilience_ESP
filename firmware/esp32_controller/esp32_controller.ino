#include <Arduino.h>

#include "led_engine.h"
#include "led_expander_output.h"
#include "led_layout.h"
#include "led_settings.h"
#include "led_state.h"
#include "web_setup.h"

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
const unsigned long OLED_SETUP_PAGE_INTERVAL_MS = 4000;

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

// GPIO40 was reserved for setup-mode experiments.
// The Tardi web controller now starts automatically while powered.
const int WEB_SETUP_BUTTON_PIN = 40;

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
// LED Output Expander UART TX is planned for GPIO39.
// Real Output Expander output remains guarded; the California validation build
// allows it at compile time, but runtime LED mode still boots OFF.

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
String serialLedCommandBuffer = "";

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
  ledSettingsBegin();
  ledEngineBegin();
  ledExpanderOutputBegin();
  webSetupBegin(true, Serial);
  setupOled();
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  processSerialLedCommands();
  readButtons();
  updateButtonDebounce();
  updateInteractionLogic();
  updateLedTriggers();
  updateLedOutputs();
  updateFireOutputs();
  printSerialDebugIfDue();
  updateOled();
  webSetupLoop();

  // No blocking delay here.
}

// ==================================================
// PIN SETUP
// ==================================================

void setupPins() {
  pinMode(WEB_SETUP_BUTTON_PIN, INPUT_PULLUP);

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

bool isWebSetupButtonHeld() {
  return digitalRead(WEB_SETUP_BUTTON_PIN) == LOW;
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

void processSerialLedCommands() {
  while (Serial.available() > 0) {
    char incoming = static_cast<char>(Serial.read());

    if (incoming == '\n' || incoming == '\r') {
      if (serialLedCommandBuffer.length() > 0) {
        handleSerialLedCommand(serialLedCommandBuffer);
        serialLedCommandBuffer = "";
      }
      continue;
    }

    if (incoming >= 32 && incoming <= 126 && serialLedCommandBuffer.length() < 80) {
      serialLedCommandBuffer += incoming;
    }
  }
}

void handleSerialLedCommand(String command) {
  command.trim();
  command.toLowerCase();

  if (command.length() == 0) {
    return;
  }

  if (command == "wifi status") {
    webSetupPrintStatus(Serial);
    return;
  }

  if (command == "led status") {
    ledExpanderOutputPrintRuntimeStatus(Serial);
    ledSettingsPrint(Serial);
    return;
  }

  if (command == "led settings") {
    ledSettingsPrint(Serial);
    return;
  }

  if (command == "led save") {
    Serial.println(ledSettingsSave() ? "LED SETTINGS saved" : "LED SETTINGS save failed");
    ledSettingsPrint(Serial);
    return;
  }

  if (command == "led defaults") {
    ledSettingsResetToDefaults();
    Serial.println("LED SETTINGS defaults loaded in RAM");
    ledSettingsPrint(Serial);
    return;
  }

  if (command == "led defaults save") {
    Serial.println(
      ledSettingsResetSavedToDefaults()
        ? "LED SETTINGS defaults saved"
        : "LED SETTINGS defaults save failed"
    );
    ledSettingsPrint(Serial);
    return;
  }

  if (command.startsWith("led set ")) {
    handleLedSettingsSetCommand(command);
    return;
  }

  if (command == "led off") {
    ledExpanderOutputSetMode(LED_OUTPUT_OFF, Serial);
    return;
  }

  if (command == "led solid") {
    ledExpanderOutputSetMode(LED_OUTPUT_VALIDATE_SOLID, Serial);
    return;
  }

  if (command == "led red") {
    ledExpanderOutputSetColorValidationMode(LED_VALIDATION_COLOR_RED, Serial);
    return;
  }

  if (command == "led green") {
    ledExpanderOutputSetColorValidationMode(LED_VALIDATION_COLOR_GREEN, Serial);
    return;
  }

  if (command == "led blue") {
    ledExpanderOutputSetColorValidationMode(LED_VALIDATION_COLOR_BLUE, Serial);
    return;
  }

  if (command == "led animation") {
    ledExpanderOutputSetMode(LED_OUTPUT_ANIMATION, Serial);
    return;
  }

  if (command.startsWith("led ch ")) {
    uint8_t channelId = 0;
    if (parseLedChannelCommand(command, channelId)) {
      ledExpanderOutputSetChannelValidationMode(channelId, Serial);
    } else {
      Serial.println("Use: led ch 0..7");
    }
    return;
  }

  if (command == "led help") {
    printLedCommandHelp();
    return;
  }

  if (command.startsWith("led")) {
    Serial.println("Unknown LED command. Use: led help");
  }
}

bool parseLedChannelCommand(const String &command, uint8_t &channelId) {
  String value = command.substring(7);
  value.trim();

  if (value.length() != 1 || value[0] < '0' || value[0] > '7') {
    return false;
  }

  channelId = static_cast<uint8_t>(value[0] - '0');
  return true;
}

bool parseLedByteValue(const String &text, uint8_t &value) {
  String trimmed = text;
  trimmed.trim();

  if (trimmed.length() == 0) {
    return false;
  }

  for (uint8_t i = 0; i < trimmed.length(); i++) {
    if (!isDigit(trimmed[i])) {
      return false;
    }
  }

  long parsed = trimmed.toInt();

  if (parsed < 0 || parsed > 255) {
    return false;
  }

  value = static_cast<uint8_t>(parsed);
  return true;
}

bool parseLedSettingsZoneCommand(
  const String &command,
  uint8_t &zoneIndex,
  uint8_t &value
) {
  String args = command.substring(String("led set zone ").length());
  args.trim();

  int separator = args.indexOf(' ');

  if (separator < 0) {
    return false;
  }

  String zoneText = args.substring(0, separator);
  String valueText = args.substring(separator + 1);
  zoneText.trim();
  valueText.trim();

  if (zoneText.length() != 1 || zoneText[0] < '0' || zoneText[0] > '7') {
    return false;
  }

  if (!parseLedByteValue(valueText, value)) {
    return false;
  }

  zoneIndex = static_cast<uint8_t>(zoneText[0] - '0');
  return true;
}

void handleLedSettingsSetCommand(const String &command) {
  LedSettings &settings = ledSettingsMutable();
  uint8_t value = 0;

  if (command.startsWith("led set brightness ")) {
    if (!parseLedByteValue(command.substring(String("led set brightness ").length()), value)) {
      Serial.println("Use: led set brightness 0..255");
      return;
    }

    settings.masterBrightness = value;
    Serial.println("LED SETTINGS brightness updated in RAM");
    ledSettingsPrint(Serial);
    return;
  }

  if (command.startsWith("led set saturation ")) {
    if (!parseLedByteValue(command.substring(String("led set saturation ").length()), value)) {
      Serial.println("Use: led set saturation 0..255");
      return;
    }

    settings.saturationScale = value;
    Serial.println("LED SETTINGS saturation updated in RAM");
    ledSettingsPrint(Serial);
    return;
  }

  if (command.startsWith("led set ambient ")) {
    if (!parseLedByteValue(command.substring(String("led set ambient ").length()), value)) {
      Serial.println("Use: led set ambient 0..255");
      return;
    }

    settings.ambientLevel = value;
    Serial.println("LED SETTINGS ambient updated in RAM");
    ledSettingsPrint(Serial);
    return;
  }

  if (command.startsWith("led set active ")) {
    if (!parseLedByteValue(command.substring(String("led set active ").length()), value)) {
      Serial.println("Use: led set active 0..255");
      return;
    }

    settings.activeLevel = value;
    Serial.println("LED SETTINGS active updated in RAM");
    ledSettingsPrint(Serial);
    return;
  }

  if (command.startsWith("led set zone ")) {
    uint8_t zoneIndex = 0;

    if (!parseLedSettingsZoneCommand(command, zoneIndex, value)) {
      Serial.println("Use: led set zone 0..7 0..255");
      return;
    }

    settings.zoneBrightness[zoneIndex] = value;
    Serial.println("LED SETTINGS zone updated in RAM");
    ledSettingsPrint(Serial);
    return;
  }

  Serial.println("Use: led set brightness|saturation|ambient|active 0..255, or led set zone 0..7 0..255");
}

void printLedCommandHelp() {
  Serial.println("LED commands:");
  Serial.println("  wifi status");
  Serial.println("  led status");
  Serial.println("  led settings");
  Serial.println("  led save");
  Serial.println("  led defaults");
  Serial.println("  led defaults save");
  Serial.println("  led set brightness 0..255");
  Serial.println("  led set saturation 0..255");
  Serial.println("  led set ambient 0..255");
  Serial.println("  led set active 0..255");
  Serial.println("  led set zone 0..7 0..255");
  Serial.println("  led off");
  Serial.println("  led solid");
  Serial.println("  led red");
  Serial.println("  led green");
  Serial.println("  led blue");
  Serial.println("  led ch 0..7");
  Serial.println("  led animation");
  Serial.println("  led help");
}

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
//
// Preserved older OLED arrangement notes:
//
// Earlier planning used a simulator/live-mode page shape with a top mode line,
// a controller status line, Input, Output, LED, and a final live-output line.
// Keep these notes here so the old arrangement is not lost while the current
// setup page work evolves.
//
// Older idle simulator page:
//   SIMULATOR MODE
//   READY
//   Input: -
//   Output: OFF
//   LED: -
//   No live output
//
// Older firing page example:
//   SIMULATOR MODE
//   FIRING
//   Input: 4
//   Output: 4
//   LED: 4
//   No live output
//
// Older pulse-complete page example:
//   SIMULATOR MODE
//   PULSE COMPLETE
//   Input: 4
//   Output: OFF
//   LED: 4
//   No live output
//
// Older Big Poof page example:
//   SIMULATOR MODE
//   BIG POOF
//   Input: 1+8
//   Output: 1 8 9
//   LED: BIG
//   No live output
//
// Older live-output wording:
//   LIVE MODE
//   FIRING
//   Input: 4
//   Output: 4
//   LED: 4
//   Live output: ON
//
// Current OLED behavior below keeps FIRE/button diagnostics as the priority
// page, and rotates in the LED Output Expander setup page only while idle.

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
  display.println(isOledLiveMode() ? "LIVE" : "SIMULATED");
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

  if (
    !hasAnyActiveInput()
    && !hasAnyActiveFireOutput()
    && shouldAllowOledSetupPage()
    && shouldShowOledSetupPage(now)
  ) {
    drawOledSetupPage();
    display.display();
    return;
  }

  drawOledControllerPage();
  display.display();
#endif
}

#if ENABLE_OLED_HARDWARE
bool isOledLiveMode() {
  return ledExpanderOutputRealOutputStarted()
    && ledExpanderOutputMode() != LED_OUTPUT_OFF;
}

void drawOledControllerModeLine() {
  if (!isOledLiveMode()) {
    drawOledLine(0, "SIMULATED");
    return;
  }

  const bool invertLive =
      ((millis() / (OLED_UPDATE_INTERVAL_MS * 4)) % 2) == 1;

  if (invertLive) {
    const int lineHeight = 10;
    display.fillRect(0, 0, OLED_WIDTH, lineHeight, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("LIVE");
    display.setTextColor(SSD1306_WHITE);
    return;
  }

  drawOledLine(0, "LIVE");
}

void drawOledControllerPage() {
  drawOledControllerModeLine();
  drawOledLine(1, getOledStatusLabel());

  if (hasAnyActiveInput()) {
    drawOledLineWithValue(2, "Input:", getInputDisplayLabel());
  } else {
    drawOledLine(2, "Input: -");
  }

  if (hasAnyActiveFireOutput()) {
    drawOledLineWithValue(3, "FIRE:", getOutputDisplayLabel());
  } else {
    drawOledLine(3, "FIRE: OFF");
  }

  drawOledLineWithValue(4, "LED:", getLedDisplayLabel());
}

void drawOledSetupPage() {
  if (webSetupIsActive()) {
    drawOledLine(0, "WIFI SETUP");
    drawOledLine(1, webSetupSsid());
    drawOledLine(2, webSetupIpAddress());
    return;
  }

  drawOledLine(0, "SETUP");
  drawOledLine(1, getLedUartDisplayLabel());
  drawOledLine(2, getLedUartTxDisplayLabel());
  drawOledLine(3, getRealLedDisplayLabel());
}

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

bool shouldShowOledSetupPage(unsigned long now) {
  return ((now / OLED_SETUP_PAGE_INTERVAL_MS) % 2) == 1;
}

bool shouldAllowOledSetupPage() {
  LedOutputMode mode = ledExpanderOutputMode();

  if (
    mode == LED_OUTPUT_VALIDATE_SOLID
    || mode == LED_OUTPUT_VALIDATE_CHANNEL
    || mode == LED_OUTPUT_VALIDATE_COLOR
  ) {
    return true;
  }

  if (
    mode == LED_OUTPUT_ANIMATION
    && ledExpanderOutputRealOutputAllowed()
    && ledExpanderOutputRealOutputStarted()
  ) {
    return false;
  }

  return true;
}

String getLedUartDisplayLabel() {
  LedOutputMode mode = ledExpanderOutputMode();

  if (mode == LED_OUTPUT_VALIDATE_SOLID) {
    return "LED TEST SOLID";
  }

  if (mode == LED_OUTPUT_VALIDATE_CHANNEL) {
    String label = "LED TEST CH";
    label += String(ledExpanderOutputValidationChannel());
    return label;
  }

  if (mode == LED_OUTPUT_VALIDATE_COLOR) {
    String label = "LED TEST ";
    label += String(ledExpanderOutputValidationColorName());
    label.toUpperCase();
    return label;
  }

  if (mode == LED_OUTPUT_ANIMATION) {
    return "LED ANIMATION";
  }

  if (!ledExpanderOutputRealOutputAllowed()) {
    return "LED UART OFF";
  }

  if (ledExpanderOutputRealOutputStarted()) {
    return "LED UART ON";
  }

  return "LED UART WAIT";
}

String getLedUartTxDisplayLabel() {
  String label = "TX GPIO";
  label += String(ledExpanderOutputPlannedTxPin());
  label += " ";
  label += String(ledExpanderOutputPlannedBaudRate() / 1000000);
  label += "M";
  return label;
}

String getRealLedDisplayLabel() {
  if (
    ledExpanderOutputMode() != LED_OUTPUT_OFF
    && ledExpanderOutputRealOutputAllowed()
    && ledExpanderOutputRealOutputStarted()
  ) {
    if (ledExpanderOutputMode() == LED_OUTPUT_VALIDATE_COLOR) {
      String label = "CHECK LEDS ";
      label += String(ledExpanderOutputValidationColorName());
      label.toUpperCase();
      return label;
    }

    return "CHECK LEDS";
  }

  return "Real LEDs OFF";
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
    return "BIG POOF";
  }

  LedOutputMode mode = ledExpanderOutputMode();

  if (mode == LED_OUTPUT_VALIDATE_SOLID) {
    return "TEST ALL";
  }

  if (mode == LED_OUTPUT_VALIDATE_CHANNEL) {
    String label = "TEST CH";
    label += String(ledExpanderOutputValidationChannel());
    return label;
  }

  if (mode == LED_OUTPUT_VALIDATE_COLOR) {
    String label = "TEST ";
    label += String(ledExpanderOutputValidationColorName());
    label.toUpperCase();
    return label;
  }

  String label = "";
  unsigned long now = millis();

  for (int i = 0; i < 7; i++) {
    if (ledIsZoneActive(i, now)) {
      if (label.length() > 0) {
        label += "+";
      }

      label += String(i + 1);

      if (label.length() > 9) {
        return "MULTI ANIMATION";
      }
    }
  }

  if (label.length() > 0) {
    if (label.indexOf('+') < 0) {
      return String("ZONE ") + label + " ANIMATION";
    }

    label = String("ZONES ") + label + " ANIM";

    if (label.length() > 16) {
      return "MULTI ANIMATION";
    }

    return label;
  }

  if (mode == LED_OUTPUT_ANIMATION) {
    if (ledExpanderOutputRealOutputAllowed() && !ledExpanderOutputRealOutputStarted()) {
      return "WAIT";
    }

    return "AMBIENT";
  }

  return "OFF";
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
