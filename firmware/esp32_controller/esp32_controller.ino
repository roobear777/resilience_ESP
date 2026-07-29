#include <Arduino.h>

#if defined(ESP32)
#include "soc/gpio_struct.h"
#endif

#include "led_engine.h"
#include "led_direct_output.h"
#include "led_layout.h"
#include "led_settings.h"
#include "led_state.h"
#include "web_setup.h"

#if !ARDUINO_USB_CDC_ON_BOOT
#error "Enable Tools > USB CDC On Boot so Serial uses native USB GPIO19/GPIO20; UART0 conflicts with the Z7 LED lane on GPIO43."
#endif

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
// - FIRE9 / Head Poof active only while Button 1 + Button 8 are held
// - Serial diagnostics
//
// Full baseline details live in docs/current_baseline.md.
// Pin mapping rules live in docs/pin_mapping.md.

// ==================================================
// CONFIG
// ==================================================

const int NUM_BUTTONS = 8;
const int NUM_FIRE_OUTPUTS = 9;

const int FIRE9_INDEX = 8;

const unsigned long DEBOUNCE_MS = 30;

// FIRE1-FIRE8: 100 ms repeating pulse while held.
// FIRE9 / Head Poof follows the Button 1 + Button 8 hold state.
const unsigned long NORMAL_FIRE_PULSE_MS = 100;
const unsigned long FIRE_REPEAT_INTERVAL_MS = 1000;
const unsigned long ALL_BUTTONS_FIRE_PULSE_MS = 500;

// Backup guard only.
// Normal pulse behavior should return outputs HIGH after 100 ms.
const unsigned long OUTPUT_CUTOFF_MS = 10000;

// Prevents Serial flooding while keeping the loop non-blocking.
const unsigned long SERIAL_DEBUG_INTERVAL_MS = 100;

const uint8_t RGB_TEST_GPIO38_PIN = 38;
const uint8_t RGB_TEST_GPIO48_PIN = 48;

// false = keep FIRE GPIOs idle HIGH; Serial still shows requested state.
// true  = allow FIRE GPIOs to drive the live active-LOW FIRE outputs.
const bool FIRE_OUTPUTS_ENABLED = true;

// ==================================================
// INPUT PULLDOWN MODE
// ==================================================
//
// true  = use ESP32 internal pull-downs for development wiring.
// false = use live hardware external 10k pull-downs.
//
// Logical behavior is unchanged:
// LOW  = inactive
// HIGH = active
//
// Preferred button wiring:
// ESP32 3.3V -> button panel -> button return wire -> ESP32 GPIO input
const bool USE_INTERNAL_PULLDOWNS = false;

// ==================================================
// PIN CONFIG
// ==================================================
//
// Current board:
// ESP32-S3-DevKitC-1-N8R8
//
// Current live-build pin map.
//
// Keep all raw GPIO numbers in this section.
//
// Reserved / internally owned:
// - GPIO0: FastLED LCD_CLOCKLESS dummy/padding signal; leave unwired
// - GPIO19/GPIO20: native USB D-/D+
// - GPIO48: likely onboard RGB/status LED related
//
// Direct FastLED data lanes are GPIO1, GPIO2, GPIO39, GPIO40, GPIO41,
// GPIO42, and GPIO43 for Z1 through Z7. OLED support and Pixelblaze Output
// Expander output have been removed. Direct ambient animation output starts
// automatically after setup completes.

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
unsigned long firePulseDurationMs[NUM_FIRE_OUTPUTS] = { 0 };
unsigned long lastFireRepeatMs[NUM_BUTTONS] = { 0 };

unsigned long bigPoofStartMs = 0;
unsigned long allButtonsPulseStartMs = 0;
bool allButtonsPulseActive = false;
bool allButtonsPulseArmed = true;

unsigned long lastSerialDebugMs = 0;

String serialLedCommandBuffer = "";

void startRgbTestDiagnostic();
void stopRgbTestDiagnostic();
void rgbDiagnosticSetCandidatePinsInput();
void rgbDiagnosticShowRed(uint8_t pin);
void rgbDiagnosticShowGreen(uint8_t pin);
void rgbDiagnosticShowOff(uint8_t pin);
void rgbDiagnosticWritePixel(uint8_t pin, uint8_t red, uint8_t green, uint8_t blue);
void rgbDiagnosticWriteByte(uint8_t pin, uint8_t value);
void rgbDiagnosticWriteBit(uint8_t pin, bool value);
void rgbDiagnosticWritePinFast(uint8_t pin, bool level);
void rgbDiagnosticDelayNops(uint16_t count);

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

  ledStateBegin();
  ledSettingsBegin();
  ledEngineBegin();
  ledDirectOutputBegin();
  ledDirectOutputRunStartupHardwareTest(Serial);
  webSetupBegin(true, Serial);
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  processSerialLedCommands();
  readButtons();
  updateButtonDebounce();
  updateLedOverrides();
  bool bigPoofStartedThisLoop = updateInteractionLogic();
  updateLedTriggers(bigPoofStartedThisLoop);
  updateLedOutputs();
  updateFireOutputs();
  printSerialDebugIfDue();
  webSetupLoop();

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

bool updateInteractionLogic() {
  updateInteractionState();
  clearFireStates();
  bool suppressNormalFireLogic = updateAllButtonsFireLogic();
  bool bigPoofStartedThisLoop = false;

  if (!suppressNormalFireLogic) {
    updateNormalFireLogic();
    bigPoofStartedThisLoop = updateBigPoofLogic();
  }

  updateFirePulseStates();
  return bigPoofStartedThisLoop;
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

void startFirePulseForDuration(int fireIndex, unsigned long pulseDurationMs) {
  if (fireIndex < 0 || fireIndex >= NUM_FIRE_OUTPUTS) {
    return;
  }

  firePulseActive[fireIndex] = true;
  firePulseStartMs[fireIndex] = millis();
  firePulseDurationMs[fireIndex] = pulseDurationMs;
}

void startFirePulse(int fireIndex) {
  startFirePulseForDuration(fireIndex, NORMAL_FIRE_PULSE_MS);
}

void updateNormalFireLogic() {
  // Button 1-8 each start matching 100 ms FIRE1-FIRE8 pulses while held.
  unsigned long now = millis();

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

bool updateAllButtonsFireLogic() {
  unsigned long now = millis();
  bool allButtonsPressed = areAllButtonsPressed();

  if (!allButtonsPressed) {
    allButtonsPulseArmed = true;
  }

  if (allButtonsPulseActive) {
    if ((now - allButtonsPulseStartMs) < ALL_BUTTONS_FIRE_PULSE_MS) {
      bigPoofStartMs = 0;
      syncNormalFireRepeatTimers(now);
      return true;
    }

    allButtonsPulseActive = false;
    allButtonsPulseStartMs = 0;
    bigPoofStartMs = 0;
    syncNormalFireRepeatTimers(now);
  }

  if (allButtonsPressed && allButtonsPulseArmed) {
    allButtonsPulseActive = true;
    allButtonsPulseArmed = false;
    allButtonsPulseStartMs = now;
    bigPoofStartMs = 0;

    for (int i = 0; i < NUM_FIRE_OUTPUTS; i++) {
      startFirePulseForDuration(i, ALL_BUTTONS_FIRE_PULSE_MS);
    }

    syncNormalFireRepeatTimers(now);
    return true;
  }

  if (allButtonsPressed && !allButtonsPulseArmed) {
    bigPoofStartMs = 0;
    syncNormalFireRepeatTimers(now);
    return true;
  }

  return false;
}

bool areAllButtonsPressed() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!debouncedButtonState[i]) {
      return false;
    }
  }

  return true;
}

void syncNormalFireRepeatTimers(unsigned long now) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    lastFireRepeatMs[i] = debouncedButtonState[i] ? now : 0;
  }
}

// ==================================================
// HEAD POOF LOGIC
// ==================================================
//
// Current trigger:
// Button 1 + Button 8 pressed together -> FIRE9
//
// Current behavior:
// FIRE9 is active only while Button 1 and Button 8 are both held.
// Releasing either input turns FIRE9 off.
// A 10 second cutoff still prevents a stuck Head Poof output.

bool isBigPoofRequested() {
  return debouncedButtonState[0] && debouncedButtonState[7];
}

bool updateBigPoofLogic() {
  bool bigPoofRequested = isBigPoofRequested();

  if (!bigPoofRequested) {
    bigPoofStartMs = 0;
    fireState[FIRE9_INDEX] = false;
    return false;
  }

  unsigned long now = millis();
  bool bigPoofStartedThisLoop = false;

  if (bigPoofStartMs == 0) {
    bigPoofStartMs = now;
    ledActivateAllZones(now);
    bigPoofStartedThisLoop = true;
  }

  if ((now - bigPoofStartMs) < OUTPUT_CUTOFF_MS) {
    fireState[FIRE9_INDEX] = true;
  } else {
    fireState[FIRE9_INDEX] = false;
  }

  return bigPoofStartedThisLoop;
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
      firePulseDurationMs[i] = 0;
      fireState[i] = false;
    } else {
      fireState[i] = true;
    }
  }
}

unsigned long getPulseDurationMs(int fireIndex) {
  if (fireIndex >= 0 && fireIndex < NUM_FIRE_OUTPUTS && firePulseDurationMs[fireIndex] > 0) {
    return firePulseDurationMs[fireIndex];
  }

  return NORMAL_FIRE_PULSE_MS;
}

// ==================================================
// LED HOOKS
// ==================================================

void updateLedOverrides() {
  ledEngineSetAllGreenOverride(debouncedButtonState[1] && debouncedButtonState[5]);
}

void updateLedTriggers(bool bigPoofStartedThisLoop) {
  unsigned long now = millis();

  if (buttonPressEvent[0] && !bigPoofStartedThisLoop) {
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
  ledDirectOutputUpdate(now);
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

  if (command == "rgb test") {
    startRgbTestDiagnostic();
    return;
  }

  if (command == "rgb off") {
    stopRgbTestDiagnostic();
    return;
  }

  if (command == "led status") {
    ledDirectOutputPrintRuntimeStatus(Serial);
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
    ledDirectOutputSetMode(LED_OUTPUT_OFF, Serial);
    return;
  }

  if (command == "led solid") {
    ledDirectOutputSetMode(LED_OUTPUT_VALIDATE_SOLID, Serial);
    return;
  }

  if (command == "led red") {
    ledDirectOutputSetColorValidationMode(LED_VALIDATION_COLOR_RED, Serial);
    return;
  }

  if (command == "led green") {
    ledDirectOutputSetColorValidationMode(LED_VALIDATION_COLOR_GREEN, Serial);
    return;
  }

  if (command == "led blue") {
    ledDirectOutputSetColorValidationMode(LED_VALIDATION_COLOR_BLUE, Serial);
    return;
  }

  if (command == "led animation") {
    ledDirectOutputSetMode(LED_OUTPUT_ANIMATION, Serial);
    return;
  }

  if (command.startsWith("led ch ")) {
    uint8_t channelId = 0;
    if (parseLedChannelCommand(command, channelId)) {
      ledDirectOutputSetLaneValidationMode(channelId, Serial);
    } else {
      Serial.println("Use: led ch 1..7");
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

  if (value.length() != 1 || value[0] < '1' || value[0] > '7') {
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

  if (
    zoneText.length() != 1
    || zoneText[0] < '0'
    || zoneText[0] >= static_cast<char>('0' + LED_LOGICAL_ZONE_COUNT)
  ) {
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
      Serial.println("Use: led set zone 0..6 0..255");
      return;
    }

    settings.zoneBrightness[zoneIndex] = value;
    Serial.println("LED SETTINGS zone updated in RAM");
    ledSettingsPrint(Serial);
    return;
  }

  Serial.println("Use: led set brightness|saturation|ambient|active 0..255, or led set zone 0..6 0..255");
}

void printLedCommandHelp() {
  Serial.println("LED commands:");
  Serial.println("  wifi status");
  Serial.println("  rgb test");
  Serial.println("  rgb off");
  Serial.println("  led status");
  Serial.println("  led settings");
  Serial.println("  led save");
  Serial.println("  led defaults");
  Serial.println("  led defaults save");
  Serial.println("  led set brightness 0..255");
  Serial.println("  led set saturation 0..255");
  Serial.println("  led set ambient 0..255");
  Serial.println("  led set active 0..255");
  Serial.println("  led set zone 0..6 0..255");
  Serial.println("  led off");
  Serial.println("  led solid");
  Serial.println("  led red");
  Serial.println("  led green");
  Serial.println("  led blue");
  Serial.println("  led ch 1..7");
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

  Serial.print(" | HeadPoof=");
  Serial.print(isBigPoofRequested() ? "1" : "0");

  Serial.print(" | Outputs=");
  Serial.print(FIRE_OUTPUTS_ENABLED ? "ON" : "OFF");

  Serial.print(" | PullDown=");
  Serial.println(USE_INTERNAL_PULLDOWNS ? "INTERNAL" : "EXTERNAL");
}

// ==================================================
// TEMPORARY ONBOARD RGB DIAGNOSTIC
// ==================================================

void startRgbTestDiagnostic() {
  rgbDiagnosticShowRed(RGB_TEST_GPIO38_PIN);
  rgbDiagnosticShowGreen(RGB_TEST_GPIO48_PIN);
  rgbDiagnosticSetCandidatePinsInput();
  Serial.println("RGB TEST:");
  Serial.println("RED = GPIO38 = DevKitC-1 v1.1");
  Serial.println("GREEN = GPIO48 = earlier/original DevKitC-1 revision");
  Serial.println("Type rgb off to clear.");
}

void stopRgbTestDiagnostic() {
  rgbDiagnosticShowOff(RGB_TEST_GPIO38_PIN);
  rgbDiagnosticShowOff(RGB_TEST_GPIO48_PIN);
  rgbDiagnosticSetCandidatePinsInput();
  Serial.println("RGB TEST OFF");
}

void rgbDiagnosticSetCandidatePinsInput() {
  pinMode(RGB_TEST_GPIO38_PIN, INPUT);
  pinMode(RGB_TEST_GPIO48_PIN, INPUT);
}

void rgbDiagnosticShowRed(uint8_t pin) {
  rgbDiagnosticWritePixel(pin, 255, 0, 0);
}

void rgbDiagnosticShowGreen(uint8_t pin) {
  rgbDiagnosticWritePixel(pin, 0, 255, 0);
}

void rgbDiagnosticShowOff(uint8_t pin) {
  rgbDiagnosticWritePixel(pin, 0, 0, 0);
}

void rgbDiagnosticWritePixel(uint8_t pin, uint8_t red, uint8_t green, uint8_t blue) {
  pinMode(pin, OUTPUT);
  rgbDiagnosticWritePinFast(pin, LOW);
  delayMicroseconds(80);

  noInterrupts();
  rgbDiagnosticWriteByte(pin, green);
  rgbDiagnosticWriteByte(pin, red);
  rgbDiagnosticWriteByte(pin, blue);
  interrupts();

  rgbDiagnosticWritePinFast(pin, LOW);
  delayMicroseconds(80);
}

void rgbDiagnosticWriteByte(uint8_t pin, uint8_t value) {
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    rgbDiagnosticWriteBit(pin, (value & mask) != 0);
  }
}

void rgbDiagnosticWriteBit(uint8_t pin, bool value) {
  rgbDiagnosticWritePinFast(pin, HIGH);

  if (value) {
    rgbDiagnosticDelayNops(62);
    rgbDiagnosticWritePinFast(pin, LOW);
    rgbDiagnosticDelayNops(28);
  } else {
    rgbDiagnosticDelayNops(28);
    rgbDiagnosticWritePinFast(pin, LOW);
    rgbDiagnosticDelayNops(62);
  }
}

void rgbDiagnosticWritePinFast(uint8_t pin, bool level) {
#if defined(ESP32)
  if (pin < 32) {
    if (level) {
      GPIO.out_w1ts = 1UL << pin;
    } else {
      GPIO.out_w1tc = 1UL << pin;
    }
    return;
  }

  if (level) {
    GPIO.out1_w1ts.val = 1UL << (pin - 32);
  } else {
    GPIO.out1_w1tc.val = 1UL << (pin - 32);
  }
#else
  digitalWrite(pin, level ? HIGH : LOW);
#endif
}

void rgbDiagnosticDelayNops(uint16_t count) {
  while (count-- > 0) {
    asm volatile("nop");
  }
}
