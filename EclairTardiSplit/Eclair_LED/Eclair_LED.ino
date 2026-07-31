// =============================================================================
// ECLAIR — LED CONTROLLER
// ESP32-S3, Resilience tardigrade build
// =============================================================================
//
// One half of the two-board split. Eclair does LEDs and nothing else:
//
//   - 8 debounced button inputs (the SAME physical wires that feed Tardi)
//   - the Z1-Z8 zone animation engine, unchanged from the July 24 firmware
//   - 8 parallel WS2812 lanes via FastLED LCD_CLOCKLESS
//   - the TARDI-LED WiFi tuning page
//
// It has NO fire code. It cannot drive a relay. That is Tardi's job, on its
// own board, with no WiFi stack anywhere near it.
//
// -----------------------------------------------------------------------------
// WHY TWO BOARDS
// -----------------------------------------------------------------------------
//
// The electronics enclosure is full, so the LED driver had to move to a second
// box anyway. Given that, splitting by FUNCTION rather than by zone gets three
// things for free:
//
//   1. No inter-board link to fail. The button wires land on BOTH boards, so
//      the buttons themselves are the synchronisation mechanism. Both boards
//      see the same edge at the same instant. There is no I2C, no UART, no
//      protocol between them — if the "link" breaks, a button has broken, and
//      you find that out with a meter in ten seconds.
//
//   2. All eight zones stay on ONE chip, so cross-zone animations (the
//      peristaltic wave, the full-body payoff) need no phase-locking. This is
//      the difference between this split and the earlier "3 zones here, 4
//      zones there" proposal, which would have needed exactly that.
//
//   3. Fire gets a dedicated controller running a few hundred lines with no
//      WiFi, no DMA and no 22 ms frame loop. That is a real safety gain.
//
// -----------------------------------------------------------------------------
// Z8 IS BACK
// -----------------------------------------------------------------------------
//
// The single-board direct-drive build had to drop Z8 (the button-station
// strings) because GPIO1/2 were the OLED and there was no eighth lane left.
// With the OLED moved to Tardi, GPIO38 is free and Z8 has a real lane again.
//
// -----------------------------------------------------------------------------
// ARDUINO IDE
// -----------------------------------------------------------------------------
//   Board            : ESP32S3 Dev Module
//   USB CDC On Boot  : ENABLED   <-- required, GPIO43 is LED lane Z7
//   PSRAM            : OPI PSRAM
//   Flash Size       : 8MB
//   Plug into the USB port, not the UART port.
// =============================================================================

#include <Arduino.h>

#include "led_direct_output.h"
#include "led_engine.h"
#include "led_layout.h"
#include "led_settings.h"
#include "led_state.h"
#include "web_setup.h"

// =============================================================================
// CONFIGURATION
// =============================================================================

const int NUM_BUTTONS = 8;

// A raw reading must hold steady for this long before we believe it. Anything
// shorter is treated as noise and ignored. 30 ms matches Tardi.
const unsigned long DEBOUNCE_MS = 30;

// Status banner repeat. Printed on a timer rather than once at boot, so it is
// on screen whenever you open the Serial Monitor rather than 90 seconds gone.
const unsigned long STATUS_BANNER_INTERVAL_MS = 5000;

// Bench safety. An input pin with nothing wired to it FLOATS, and a floating
// high-impedance CMOS input is an antenna — it will read random highs,
// especially sitting next to eight lines switching at 800 kHz. That looks
// exactly like spurious triggering, and no amount of debounce fixes it
// because the pin genuinely is high.
//
// The internal pulldown (~45k) holds an unwired pin low. In production it
// simply parallels with the external 10k at the button end, which is harmless.
//
// NOTE: pulldown, never pullup. These buttons are active-HIGH.
const bool USE_INTERNAL_PULLDOWNS = true;

// Marks a button that has no LED zone of its own.
const uint8_t NO_ZONE = 255;

// =============================================================================
// PER-BUTTON DATA
// =============================================================================
//
// Same shape as the per-strip struct in picoV3_esp32: one struct per input
// holding its pin, its debounce state and its diagnostics, with a single
// function that handles everything for one button. Easier to reason about
// than eight parallel arrays indexed in lockstep.
//
// One deliberate difference from picoV3: this struct owns the BUTTON only,
// not the zone's active/ambient state. That lives in led_state.cpp, which is
// the single owner of "is this zone animating right now" and is also what the
// animation engine and the web UI read. Duplicating it here would create two
// sources of truth for the same fact.
//
// picoV3 could keep both in one struct because a strip there was just a strip.
// Here a zone has its own engine, settings and duration, so the split matters.

struct ZoneButton {
  int pin;                       // GPIO — same number on Tardi
  uint8_t zoneIndex;             // LED zone it triggers, or NO_ZONE
  const char *label;

  // --- debounce ---
  bool stableReading;            // the level we currently believe
  bool rawReading;               // most recent raw sample
  unsigned long rawChangedAt;    // when rawReading last changed

  // --- diagnostics ---
  unsigned long triggerCount;    // accepted rising edges since boot
  unsigned long glitchCount;     // raw changes rejected as too short
};

// Button pins are DELIBERATELY the same GPIO numbers Tardi uses. One physical
// wire per button lands on the identically-numbered pin of both boards. Keep
// this invariant — it is what makes the two-board split debuggable.
//
// LED lane pins live in led_direct_output.cpp:
//   Z1 GPIO1   Z2 GPIO2   Z3 GPIO39  Z4 GPIO40
//   Z5 GPIO41  Z6 GPIO42  Z7 GPIO43  Z8 GPIO38
//
// GPIO0 is claimed by the LCD driver as an internal padding pin. Unwired.
// Free after all of the above: GPIO3, GPIO44.

ZoneButton buttons[NUM_BUTTONS] = {
  {  4, LED_ZONE_Z1_MOUTH,      "B1 -> Z1 mouth"     },
  {  5, LED_ZONE_Z2_SHOULDER,   "B2 -> Z2 shoulder"  },
  {  6, LED_ZONE_Z3_MIDBODY,    "B3 -> Z3 midbody"   },
  {  7, LED_ZONE_Z4_REAR,       "B4 -> Z4 rear"      },
  { 15, LED_ZONE_Z5_FRONT_LEGS, "B5 -> Z5 frontlegs" },
  { 16, LED_ZONE_Z6_BACK_LEGS,  "B6 -> Z6 backlegs"  },
  { 17, LED_ZONE_Z7_DIGESTIVE,  "B7 -> Z7 digestive" },
  { 18, NO_ZONE,                "B8 -> fire only"    },
};

unsigned long lastStatusBannerMs = 0;
String serialCommandBuffer = "";

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);                      // let native USB CDC enumerate

  Serial.println();
  Serial.println("=====================================");
  Serial.println(" ECLAIR - LED CONTROLLER");
  Serial.println(" no fire outputs on this board");
  Serial.println("=====================================");

  setupButtons();

  ledSettingsBegin();
  ledStateBegin();
  ledEngineBegin();
  ledDirectOutputBegin();

  // WiFi lives HERE, on the LED board — never on the fire board.
  webSetupBegin(true, Serial);

  Serial.println();
  ledDirectOutputPrintDriverTable(Serial);
  Serial.println();
  printCommandHelp();
}

void setupButtons() {
  for (int b = 0; b < NUM_BUTTONS; b++) {
    ZoneButton &btn = buttons[b];

    pinMode(btn.pin, USE_INTERNAL_PULLDOWNS ? INPUT_PULLDOWN : INPUT);

    btn.stableReading = false;
    btn.rawReading    = false;
    btn.rawChangedAt  = 0;
    btn.triggerCount  = 0;
    btn.glitchCount   = 0;
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  unsigned long now = millis();

  processSerialCommands();

  for (int b = 0; b < NUM_BUTTONS; b++) {
    checkButtonForZone(b, now);
  }

  updateCombos(now);

  ledEngineUpdate(now);
  ledDirectOutputUpdate(now);

  webSetupLoop();
  printStatusBannerIfDue(now);
}

// =============================================================================
// BUTTON HANDLING (debounced)
// =============================================================================
//
// A level has to hold steady for DEBOUNCE_MS before we act on it. Any change
// shorter than that is counted as a glitch and thrown away — the glitch
// counter is reported in the status banner, so if zones self-trigger you can
// see immediately whether it is electrical noise (glitches climbing) or
// something else entirely (they aren't).

void checkButtonForZone(int b, unsigned long now) {
  ZoneButton &btn = buttons[b];
  bool raw = (digitalRead(btn.pin) == HIGH);

  // The raw level moved. Restart the settling clock and wait it out.
  if (raw != btn.rawReading) {
    // If it moves again before settling, that previous move was noise.
    if (btn.rawReading != btn.stableReading) {
      btn.glitchCount++;
    }
    btn.rawReading   = raw;
    btn.rawChangedAt = now;
    return;
  }

  // Raw is steady — but has it been steady long enough to believe?
  if ((now - btn.rawChangedAt) < DEBOUNCE_MS) {
    return;
  }

  // Settled. Does it differ from the level we currently believe?
  if (raw == btn.stableReading) {
    return;
  }

  bool risingEdge = (raw && !btn.stableReading);
  btn.stableReading = raw;

  if (!risingEdge) {
    return;
  }

  btn.triggerCount++;

  // Button 1 is claimed by the full-body combo while B8 is also held, so it
  // does not additionally fire its own zone in that case.
  if (b == 0 && buttons[7].stableReading) {
    return;
  }

  if (btn.zoneIndex != NO_ZONE) {
    // ledTriggerZone RESTARTS the window rather than being ignored while the
    // zone is already active — a press always gets a response, matching the
    // retrigger behaviour in picoV3_esp32.
    ledTriggerZone(btn.zoneIndex, now);
    Serial.printf("TRIGGER: %s\n", btn.label);
  }
}

// =============================================================================
// BUTTON COMBINATIONS
// =============================================================================
//
// Combos read the debounced levels straight off the structs. Both boards
// detect these independently from the same wires, which is exactly why no
// link between them is needed.

bool isHeld(int b) {
  return buttons[b].stableReading;
}

bool isFullBodyRequested() {
  return isHeld(0) && isHeld(7);      // B1 + B8, same combo as Tardi's head poof
}

bool isGreenOverrideRequested() {
  return isHeld(1) && isHeld(5);      // B2 + B6
}

void updateCombos(unsigned long now) {
  static bool fullBodyLatched = false;

  ledEngineSetAllGreenOverride(isGreenOverrideRequested());

  if (isFullBodyRequested()) {
    if (!fullBodyLatched) {
      fullBodyLatched = true;
      ledActivateAllZones(now);
      Serial.println("TRIGGER: full body (B1+B8)");
    }
  } else {
    fullBodyLatched = false;
  }
}

// =============================================================================
// STATUS BANNER
// =============================================================================

void printStatusBannerIfDue(unsigned long now) {
  if ((now - lastStatusBannerMs) < STATUS_BANNER_INTERVAL_MS) return;
  lastStatusBannerMs = now;
  printStatusBanner(now);
}

void printStatusBanner(unsigned long now) {
  const LedSettings &settings = ledSettingsGet();

  Serial.println();
  Serial.println("=== ECLAIR - LED CONTROLLER =========================");
  ledDirectOutputPrintDriverTable(Serial);

  Serial.println();
  Serial.println("btn  gpio  pin  zone  state      trig  glitch");
  unsigned long totalGlitches = 0;
  for (int b = 0; b < NUM_BUTTONS; b++) {
    const ZoneButton &btn = buttons[b];
    totalGlitches += btn.glitchCount;

    char zone[8];
    char state[12];
    if (btn.zoneIndex == NO_ZONE) {
      strcpy(zone, "-");
      strcpy(state, "fire only");
    } else {
      snprintf(zone, sizeof(zone), "Z%d", btn.zoneIndex + 1);
      strcpy(state, ledEngineIsZoneActive(btn.zoneIndex, now) ? "ACTIVE" : "ambient");
    }

    Serial.printf("  %d  %4d   %s  %-4s  %-9s %5lu  %6lu\n",
                  b + 1, btn.pin, btn.stableReading ? "HI" : "lo",
                  zone, state, btn.triggerCount, btn.glitchCount);
  }

  // Z8 has no button of its own — it follows the full-body payoff.
  Serial.printf("  -     -    -   Z8    %-9s  (full body only)\n",
                ledEngineIsZoneActive(LED_ZONE_Z8_STATIONS, now) ? "ACTIVE" : "ambient");

  Serial.println();
  Serial.printf("combos : full-body(B1+B8) %s   green(B2+B6) %s\n",
                isFullBodyRequested() ? "YES" : "no",
                isGreenOverrideRequested() ? "YES" : "no");
  Serial.printf("buttons: debounce %lu ms, internal pulldown %s, %lu glitches rejected\n",
                DEBOUNCE_MS, USE_INTERNAL_PULLDOWNS ? "ON" : "off", totalGlitches);
  if (totalGlitches > 0) {
    Serial.println("         glitches climbing = electrical noise on the button wiring.");
    Serial.println("         Fit 1k series + 100nF to GND at each input.");
  }
  Serial.printf("look   : brightness %u  speed %u%%  palette %s  behaviour %s\n",
                settings.masterBrightness, settings.speedPercent,
                ledSettingsPaletteName(settings.paletteMode),
                ledSettingsBehaviorName(settings.behaviorMode));
  Serial.printf("wifi   : %s  %s  clients %u\n",
                webSetupSsid(), webSetupIpAddress().c_str(),
                webSetupClientCount());
  Serial.println("=====================================================");
}

// =============================================================================
// SERIAL COMMANDS
// =============================================================================

void printCommandHelp() {
  Serial.println("commands:");
  Serial.println("  status          full status banner now");
  Serial.println("  driver          lane / driver table");
  Serial.println("  heap            heap and DMA free space");
  Serial.println("  led on|off      animation or blackout");
  Serial.println("  led solid       all lanes dim white");
  Serial.println("  led ch 0..7     light ONE lane only");
  Serial.println("  led red|green|blue   solid colour (checks byte order)");
  Serial.println("  trigger 1..8    fire a zone from the keyboard");
  Serial.println("  trigger all     full-body payoff");
  Serial.println("  help");
}

void processSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialCommandBuffer.trim();
      if (serialCommandBuffer.length() > 0) {
        handleSerialCommand(serialCommandBuffer);
      }
      serialCommandBuffer = "";
    } else {
      serialCommandBuffer += c;
      if (serialCommandBuffer.length() > 64) serialCommandBuffer = "";
    }
  }
}

void handleSerialCommand(String cmd) {
  cmd.toLowerCase();
  unsigned long now = millis();

  if (cmd == "help" || cmd == "?") { printCommandHelp(); return; }

  if (cmd == "status") { printStatusBanner(now); return; }

  if (cmd == "driver") { ledDirectOutputPrintDriverTable(Serial); return; }

  if (cmd == "heap") { ledDirectOutputPrintHeap("now", Serial); return; }

  if (cmd == "led on")    { ledDirectOutputSetMode(LED_OUTPUT_ANIMATION, Serial); return; }
  if (cmd == "led off")   { ledDirectOutputSetMode(LED_OUTPUT_OFF, Serial); return; }
  if (cmd == "led solid") { ledDirectOutputSetMode(LED_OUTPUT_VALIDATE_SOLID, Serial); return; }

  if (cmd == "led red")   { ledDirectOutputSetColorValidationMode(LED_VALIDATION_COLOR_RED, Serial); return; }
  if (cmd == "led green") { ledDirectOutputSetColorValidationMode(LED_VALIDATION_COLOR_GREEN, Serial); return; }
  if (cmd == "led blue")  { ledDirectOutputSetColorValidationMode(LED_VALIDATION_COLOR_BLUE, Serial); return; }

  if (cmd.startsWith("led ch ")) {
    int lane = cmd.substring(7).toInt();
    ledDirectOutputSetChannelValidationMode((uint8_t)lane, Serial);
    return;
  }

  if (cmd == "trigger all") {
    ledActivateAllZones(now);
    Serial.println("triggered: all zones");
    return;
  }

  if (cmd.startsWith("trigger ")) {
    int zone = cmd.substring(8).toInt();
    if (zone >= 1 && zone <= 8) {
      ledTriggerZone(zone - 1, now);
      Serial.printf("triggered: Z%d\n", zone);
    } else {
      Serial.println("zone must be 1..8, or 'trigger all'");
    }
    return;
  }

  Serial.print("unknown command: ");
  Serial.println(cmd);
}
