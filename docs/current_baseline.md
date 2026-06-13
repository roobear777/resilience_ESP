# Current Baseline

This file records the confirmed working baseline for the Tardi Controller.

It describes the current ESP32-S3 controller plan and keeps only the inherited architecture details that remain relevant to the new build.

Detailed GPIO notes are tracked separately in:

```text
docs/gpio_schema.md
```

LED output details are tracked separately in:

```text
docs/esp32_led_port_status.md
docs/led_output_expander.md
docs/led_animation_architecture.md
```

Current LED port status: the ESP32 software-side render path through the Pixelblaze Output Expander simulator is implemented and passing runtime Serial diagnostics. Real Output Expander UART output compiles but is disabled by default for India-side development. See `docs/esp32_led_port_status.md` for the current handoff and California validation checklist.

## 1. Controller Target

The controller target is:

```text
Espressif ESP32-S3-DevKitC-1-N8R8
```

Current board identification:

```text
Board:  ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1-N8R8
Flash:  8 MB
PSRAM:  8 MB
ASIN:   B09MHP42LY
```

Arduino IDE board option:

```text
ESP32S3 Dev Module
```

Serial baud:

```text
115200
```

The ESP32-S3 is responsible for:

* supplying 3.3V logic voltage to the button panel
* reading 3.3V button return signals directly
* debouncing button inputs
* tracking button pressed/released state
* detecting new button press events
* running interaction logic
* calculating FIRE1-FIRE9 output states
* running one-shot FIRE pulse timing
* supporting simulation mode and live mode
* sending Serial debug output
* updating the OLED debug display
* running controller-side LED animation logic
* sending LED data to the Pixelblaze Output Expander over UART

The ESP32-S3 is the button input logic source and controller.

The ESP32-S3 does not power LED loads.

The ESP32-S3 does not power solenoids or high-current poofer hardware.

High-current LED power, poofer power, solenoids, and other load circuits remain separate from the ESP32-S3 logic supply.

## 2. Current System Summary

The inherited system is understood as:

```text
Input relays -> PixelBlaze -> poofer output relays
                          -> PixelBlaze output expander -> 8 LED strips
```

PixelBlaze currently appears to act as both:

* the input / poofer logic controller
* the LED animation controller

The new ESP32 build replaces PixelBlaze as the input / poofer logic controller.

The preferred new input architecture removes the input relay layer.

The current LED output direction is:

```text
ESP32-S3 -> UART -> standard Pixelblaze Output Expander v3.0 -> LED zones
```

In this direction, the ESP32-S3 owns controller logic and LED animation logic.

The Pixelblaze Output Expander is used as an 8-channel, 5V level-shifted LED output board.

The Pixelblaze controller board itself is not treated as the main controller in the new ESP32 architecture.

## 3. Intended Architecture Plans

### Plan A: Current Working Direction

Plan A is the current preferred controller direction.

```text
ESP32 3.3V -> button panel
button returns -> ESP32 GPIO inputs

ESP32 -> FIRE1-FIRE9 poofer output relays

ESP32-S3 -> UART -> Pixelblaze Output Expander v3.0 -> 8 LED zones
```

In Plan A:

* ESP32 is the main input and fire-output logic controller
* ESP32 sends 3.3V logic voltage out to the button panel
* each button press returns a 3.3V signal back to an ESP32 GPIO input
* input relays are removed from the preferred button input architecture
* ESP32 controls FIRE1-FIRE9 logic outputs
* ESP32 runs the LED animation logic
* ESP32 sends LED data over UART
* the standard Pixelblaze Output Expander v3.0 provides 8 level-shifted LED outputs
* LED strip power remains separate from ESP32 power

This plan supersedes the older assumption that input relays are required between the buttons and ESP32.

This plan also supersedes the older assumption that PixelBlaze must remain as the LED animation controller.

### Plan B: Later Possible LED Migration

Plan B is a later possible LED build.

```text
ESP32 3.3V -> button panel
button returns -> ESP32 GPIO inputs

ESP32 -> FIRE1-FIRE9 poofer output relays
ESP32 -> other direct LED output hardware
```

Plan B removes the Pixelblaze Output Expander.

Plan B is not confirmed.

If Plan B is used later, the ESP32 LED design must replace the 8-channel level-shifted LED output role currently assigned to the Pixelblaze Output Expander.

Plan B is a separate LED architecture task and does not block the current fire-control scaffold.

## 4. LED Output Architecture

Current LED output architecture assumes:

```text
ESP32-S3 -> UART -> standard Pixelblaze Output Expander v3.0 -> LED zones
```

The ESP32 owns controller logic and LED animation logic.

The Output Expander is used as an 8-channel, 5V level-shifted LED output board.

The ESP32-S3 sends LED data to the Output Expander over UART.

The Output Expander outputs LED data on 8 LED channels / zones.

The Output Expander does not replace LED power injection.

The Output Expander does not power the LED strips.

The LED strips still need a separate correctly sized 5V LED power supply system.

See:

```text
docs/led_output_expander.md
docs/led_animation_architecture.md
```

## 5. Button Baseline

There are 8 logical button inputs:

```text
Button 1
Button 2
Button 3
Button 4
Button 5
Button 6
Button 7
Button 8
```

Buttons use active-HIGH logic:

```text
GPIO HIGH = pressed
GPIO LOW  = released
```

Button requirements:

* 8 logical button inputs
* active-HIGH logic
* ESP32 sends 3.3V logic voltage to the button panel
* button press returns 3.3V HIGH to the matching ESP32 GPIO input
* released input is held LOW by a pull-down
* 30 ms debounce
* pressed/released state tracking
* new press event detection
* physical GPIO pin numbers are centralized and configurable

A new press event means:

```text
button was released
button becomes pressed
```

## 6. Button Input Electrical Baseline

The preferred input architecture is direct ESP32 3.3V button wiring.

The ESP32 sends 3.3V logic voltage out to the button panel.

Each button returns that 3.3V signal back to its assigned ESP32 GPIO input when pressed.

Each GPIO input is held LOW by a pull-down resistor when the button is released.

Preferred wiring:

```text
ESP32 3.3V
 |
shared 3.3V wire to button panel
 |
physical button
 |
button return wire
 |
ESP32 GPIO input
 |
10kΩ pull-down resistor
 |
GND
```

This gives reliable active-HIGH behavior:

```text
button released = 0V LOW
button pressed  = 3.3V HIGH
```

The input relay layer is not part of the preferred new button input architecture.

Older inherited assumption:

```text
5V button circuit -> input relay -> 3.3V ESP32 GPIO signal
```

New preferred assumption:

```text
ESP32 3.3V -> button panel -> button return wire -> ESP32 GPIO input
```

The inherited system may currently power the buttons from a 5V source, but this is not required for the new controller design.

The button panel may be rewired for native ESP32 3.3V input logic.

Each input signal line needs its own pull-down path to GND.

This may be implemented with individual 10kΩ resistors or with a 10kΩ resistor array.

Without a pull-down, a released button/input line could leave the GPIO input floating.

The ESP32 shares the same DC ground reference as the button input signal circuit.

For India bench testing, the firmware uses the ESP32 internal pull-downs:

```cpp
pinMode(BUTTON_PINS[i], INPUT_PULLDOWN);
```

For final California hardware, the preferred hardware baseline is external 10kΩ pull-downs with normal input mode:

```cpp
pinMode(BUTTON_PINS[i], INPUT);
```

The firmware keeps this selectable through a configuration flag.

This changes only the pin setup mode.

The logical behavior remains:

```text
LOW  = inactive
HIGH = active
```

## 7. Button Cable Direction

The preferred wiring direction is one multi-core cable between the ESP32 controller and the button panel, rather than many separate cable runs.

Likely cable structure:

```text
1 shared 3.3V wire from ESP32 to button panel
8 button return wires from button panel back to ESP32 GPIO inputs
1 shared GND
optional spare wires
```

The exact cable type and core count are not finalized.

Do not lock the design to a specific cable core count until California wiring and physical layout are confirmed.

## 8. Fire Output Baseline

There are 9 logical fire outputs:

```text
FIRE1
FIRE2
FIRE3
FIRE4
FIRE5
FIRE6
FIRE7
FIRE8
FIRE9
```

FIRE1-FIRE8 are normal fire outputs.

FIRE9 is reserved for the big poof output.

A fire output is an ESP32-controlled signal intended to trigger a relay channel for a poofer/solenoid circuit.

Poofer relay outputs use active-LOW logic:

```text
GPIO HIGH = relay inactive
GPIO LOW  = relay triggered
```

All FIRE outputs should initialize HIGH during startup.

The ESP32 pulls an output LOW for the duration of a FIRE pulse and then returns it HIGH.

The ESP32 does not directly power the solenoid.

The current schematic shows the poofer output relay side as low-voltage control signals triggering relay hardware, with the relays switching the high-power poofer/solenoid side.

The fire output architecture is independent from the button input simplification work.

The fire output architecture is also independent from the LED Output Expander work.

## 9. Normal Fire Logic

Normal test mapping:

```text
Button 1 -> FIRE1
Button 2 -> FIRE2
Button 3 -> FIRE3
Button 4 -> FIRE4
Button 5 -> FIRE5
Button 6 -> FIRE6
Button 7 -> FIRE7
Button 8 -> FIRE8
```

Latest controller mapping test result:

```text
Button 1 -> FIRE1 = PASS
Button 2 -> FIRE2 = PASS
Button 3 -> FIRE3 = PASS
Button 4 -> FIRE4 = PASS
Button 5 -> FIRE5 = PASS
Button 6 -> FIRE6 = PASS
Button 7 -> FIRE7 = PASS
Button 8 -> FIRE8 = PASS
Button 1 + Button 8 -> FIRE9 / Big Poof = PASS
```

This confirms the current firmware maps the tested button inputs to the intended FIRE outputs.

The final physical sculpture mapping will be documented separately when California wiring is confirmed.

## 10. Normal FIRE Pulse Behavior

Normal FIRE1-FIRE8 outputs use one-shot pulse behavior.

Behavior:

```text
Button press -> 500 ms FIRE pulse
Output switches LOW for the pulse duration
Output returns HIGH automatically after 500 ms
Releasing and pressing again can trigger another pulse
```

The normal FIRE1-FIRE8 pulse duration is configurable in firmware.

Current intended starting pulse duration:

```text
500 ms
```

This is the confirmed starting behavior for normal FIRE1-FIRE8 outputs.

It may still be tuned later after relay testing, flame testing, and California hardware review, but the intended behavior is one-shot 0.5 second firing.

The baseline behavior is:

```text
normal FIRE output returns HIGH automatically after the 500 ms pulse duration
```

## 11. FIRE9 / Big Poof Logic

FIRE9 is the big poof output.

The current firmware scaffold uses this big-poof trigger:

```text
Button 1 + Button 8 pressed together -> FIRE9
```

This trigger rule is isolated in one firmware function so it can be changed later without rewriting the normal FIRE1-FIRE8 logic.

Current behavior:

```text
Button 1 + Button 8 can trigger FIRE1, FIRE8, and FIRE9.
```

This preserves normal Button 1 and Button 8 behavior and adds FIRE9 as a special output.

A later revision can change the big-poof trigger to activate FIRE9 only.

## 12. FIRE9 Pulse Behavior

FIRE9 uses one-shot pulse behavior.

Behavior:

```text
Big-poof trigger requested -> FIRE9 pulse
Output switches LOW for the pulse duration
Output returns HIGH automatically after its pulse duration
Releasing and triggering again can start another FIRE9 pulse
```

FIRE9 pulse duration is configurable separately from FIRE1-FIRE8.

Current FIRE9 pulse duration:

```text
500 ms
```

FIRE9 currently also uses 500 ms, but it remains separately configurable.

FIRE9 keeps its own timing state, separate from Button 1 and Button 8 timers.

## 13. Output Cutoff / Backup Guard

The fire behavior is pulse-based, not hold-based.

The main safety behavior is:

```text
FIRE output automatically returns HIGH after its pulse duration
```

For normal FIRE1-FIRE8 outputs, that pulse duration is currently:

```text
500 ms
```

A longer cutoff remains in firmware as a backup guard against any output being held active unexpectedly.

Current backup cutoff:

```text
10 seconds
```

This cutoff is backup protection only.

It should never be reached during normal 500 ms pulse behavior.

If a future mode adds repeated poofs while a button is held, the cutoff rules must be reviewed.

## 14. Output Modes

The firmware supports simulation mode and live mode.

In code, this is controlled by:

```cpp
const bool FIRE_OUTPUTS_ENABLED = false;
```

Meaning:

```text
false = simulation mode
true  = live mode
```

### Simulation Mode

In simulation mode:

* button inputs are read
* FIRE1-FIRE9 states are calculated
* Serial debug shows intended fire outputs
* OLED shows intended fire outputs
* physical fire output pins are not activated

Simulation mode verifies button logic, OLED display, Serial debug, and pin mapping before live fire output pins are used.

### Live Mode

In live mode:

* button inputs are read
* FIRE1-FIRE9 states are calculated
* Serial debug shows fire outputs
* OLED shows fire outputs
* physical fire output pins are activated

Live mode is for later hardware commissioning and safe bench testing.

## 15. Serial Debug

Serial debug is required.

Serial debug shows:

* button pressed/released states
* debounced button states
* new press events
* FIRE1-FIRE9 output states
* active pulse states
* whether FIRE9 big poof is requested
* whether the firmware is in simulation mode or live mode
* whether internal or external input pull-down mode is selected
* LED architecture mode when LED output work is active

## 16. OLED Display

The OLED debug display is required.

The OLED shows a compact live status view, including:

* Button states
* FIRE1-FIRE9 states
* FIRE9 / big poof status
* simulation mode or live mode

Confirmed OLED pins:

```text
OLED SDA = GPIO1
OLED SCL = GPIO2
OLED VCC = 3V3
OLED GND = GND
```

OLED test displayed:

```text
Tardi Controller
OLED test OK

SDA = GPIO1
SCL = GPIO2
```

The exact OLED layout will be refined during firmware testing.

## 17. Current GPIO Baseline

The current bench-tested GPIO allocation is tracked in:

```text
docs/gpio_schema.md
```

Current temporary controller allocation:

```text
OLED SDA = GPIO1
OLED SCL = GPIO2

Button 1 = GPIO4
Button 2 = GPIO5
Button 3 = GPIO6
Button 4 = GPIO7
Button 5 = GPIO15
Button 6 = GPIO16
Button 7 = GPIO17
Button 8 = GPIO18

FIRE1 = GPIO8
FIRE2 = GPIO9
FIRE3 = GPIO10
FIRE4 = GPIO11
FIRE5 = GPIO12
FIRE6 = GPIO13
FIRE7 = GPIO14
FIRE8 = GPIO21
FIRE9 / Big Poof = GPIO47
```

Button inputs are active-HIGH:

```text
HIGH = pressed
LOW  = released
```

FIRE outputs are active-LOW:

```text
HIGH = idle / relay inactive
LOW  = trigger / relay active
```

All FIRE outputs should initialize HIGH.

The current FIRE pins have passed active-LOW LED output testing.

The current button pins have passed LOW/HIGH input testing.

These are current bench-test assignments only.

Final California wiring may still change.

## 18. Pin Mapping Rule

Physical GPIO pins must not be scattered through the interaction logic.

Pin numbers are centralized in arrays:

```cpp
const int BUTTON_PINS[8] = { ... };
const int FIRE_PINS[9] = { ... };
```

The rest of the firmware refers to logical array positions:

```text
Button 1 = BUTTON_PINS[0]
FIRE1    = FIRE_PINS[0]
```

Current temporary GPIO assignments live in:

```text
firmware/esp32_controller/esp32_controller.ino
```

These are temporary bench-test assignments only, not final California wiring.

When assigning ESP32 pins, avoid boot strapping pins and pins with special startup behavior unless they are deliberately confirmed safe.

Pins for OLED, UART LED output, and any future diagnostic signals must be reserved before final California wiring is locked.

## 19. Current Hardware Assumption

India and California use the same ESP32-S3 board family.

Pin numbers remain centralized because physical wiring can still change during testing.

The inherited schematic remains useful as a reference for:

* the previous input relay structure
* the existing PixelBlaze / output-expander LED structure
* the existing poofer relay control structure

The new working controller assumption is:

```text
ESP32 supplies 3.3V logic to the button panel
button presses return 3.3V to ESP32 GPIO inputs
released button inputs are held LOW by pull-down resistors
input relays are removed from the preferred input architecture

FIRE outputs remain separate low-voltage control signals for relay hardware
poofer relay control outputs use active-LOW logic
idle relay state is HIGH
relay trigger state is LOW

ESP32-S3 sends LED data over UART to the standard Pixelblaze Output Expander v3.0
the Output Expander provides 8 level-shifted LED output channels
LED power remains separate from ESP32 power

high-current systems remain separate from the ESP32
```

The firmware defines the new controller behavior.

## 20. Test Priorities

Latest completed tests:

```text
OLED on GPIO1/GPIO2 = PASS
Button input pins = PASS
FIRE1-FIRE9 active-LOW LED output test = PASS
Button-to-FIRE mapping test = PASS
Button 1 + Button 8 -> FIRE9 / Big Poof = PASS
```

Current remaining test priorities:

```text
1. Validate GPIO39 as 2 Mbps UART TX to the Pixelblaze Output Expander.
2. Test ESP32-S3 UART output to the Pixelblaze Output Expander.
3. Confirm LED zone mapping.
4. Confirm LED count per zone.
5. Confirm final California relay boards trigger reliably from the active-LOW ESP32 outputs.
6. Confirm whether California wiring uses the current bench GPIO allocation or needs reassignment.
```

Do not connect output pins to relay boards, solenoids, poofers, gas, flame hardware, or LED strips during this bench stage unless the specific test plan says to do so.

## 21. Current Open Questions

These still need confirmation:

```text
Does GPIO39 work reliably as 2 Mbps UART TX to the Pixelblaze Output Expander?
What exact grounding/power/reference wiring does the final Output Expander setup require?
How should LED zones 1-8 map to the physical sculpture sections?
How many LEDs are on each LED zone?
Do the final California relay boards trigger reliably when the ESP32 output is pulled LOW?
Does the California wiring match the current temporary bench GPIO allocation, or will pins need reassignment?
```
