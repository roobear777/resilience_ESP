# Pin Mapping

This file defines how logical controller names map to physical ESP32-S3 GPIO pins.

The interaction logic uses logical names such as:

```text
Button 1
FIRE1
FIRE9 / Big Poof
LED zone 1
LED BIG
```

The interaction logic should not use scattered raw GPIO numbers.

For the wider hardware baseline and architecture plan, see:

```text
docs/current_baseline.md
docs/interaction_logic.md
docs/gpio_schema.md
docs/led_output_expander.md
docs/led_animation_architecture.md
```

The temporary GPIO numbers are currently assigned in:

```text
firmware/esp32_controller/esp32_controller.ino
```

## 1. Pin Mapping Rule

All physical GPIO numbers are kept in one place in the firmware.

In the `.ino` file, this is handled with centralized constants and arrays:

```cpp
const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 2;

const int BUTTON_PINS[NUM_BUTTONS] = {
  // Button 1, Button 2, Button 3, Button 4,
  // Button 5, Button 6, Button 7, Button 8
};

const int FIRE_PINS[NUM_FIRE_OUTPUTS] = {
  // FIRE1, FIRE2, FIRE3, FIRE4, FIRE5,
  // FIRE6, FIRE7, FIRE8, FIRE9 / Big Poof
};
```

Future LED Output Expander UART pins must also be centralized before they are used.
Current planned Output Expander UART TX is GPIO39.

The rest of the code uses the arrays and logical indexes, not hardcoded GPIO numbers.

Use this pattern:

```cpp
digitalRead(BUTTON_PINS[i]);
digitalWrite(FIRE_PINS[i], firePinLevel);
```

Avoid this pattern outside the centralized pin map:

```cpp
digitalRead(4);
digitalWrite(12, state);
```

## 2. Board Target

Current controller board:

```text
Espressif ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1
Arduino IDE board option: ESP32S3 Dev Module
Serial baud: 115200
```

Confirmed upload behavior:

```text
Use Upload arrow, not the checkmark.
If upload pauses at Connecting..., hold BOOT until writing starts, then release.
After upload finishes, press RESET.
```

OLED libraries installed and working:

```text
Adafruit SSD1306
Adafruit GFX Library
```

## 3. Current Pin Status

The current `.ino` contains a temporary ESP32-S3 bench-test pin map.

That map is for local India testing and firmware development.

It is not final California wiring.

Do not treat the temporary GPIO numbers as final unless they are later confirmed with the physical build.

The ESP32 pin map currently allows for:

```text
8 button GPIO inputs
9 FIRE output GPIO pins
OLED I2C pins
future UART output to the Pixelblaze Output Expander
future diagnostic or spare pins
```

## 4. Current Bench-Test Mapping Method

GPIOs were tested using internal pull-down mode:

```cpp
pinMode(TEST_PIN, INPUT_PULLDOWN);
```

Expected input test result:

```text
GPIO open / unconnected = LOW
GPIO touched or connected to 3V3 = HIGH
```

Important distinction:

```text
Input-HIGH tested = pin reads LOW normally and HIGH when touched to 3V3.
Output-tested = pin has successfully driven a safe LED + resistor test load HIGH/LOW.
```

The current button pins have passed LOW/HIGH input testing.

The current FIRE pins have passed active-LOW LED output testing.

## 5. Current Confirmed Project Pins

| Function | GPIO | Direction | Current status | Notes |
|---|---:|---|---|---|
| OLED SDA | GPIO1 | I2C | bench-tested OK | Reserved for OLED SDA |
| OLED SCL | GPIO2 | I2C | bench-tested OK | Reserved for OLED SCL |
| Button 1 | GPIO4 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| Button 2 | GPIO5 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| Button 3 | GPIO6 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| Button 4 | GPIO7 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| Button 5 | GPIO15 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| Button 6 | GPIO16 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| Button 7 | GPIO17 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| Button 8 | GPIO18 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 |
| FIRE1 | GPIO8 | output | output-tested OK | Active-LOW output test passed |
| FIRE2 | GPIO9 | output | output-tested OK | Active-LOW output test passed |
| FIRE3 | GPIO10 | output | output-tested OK | Active-LOW output test passed |
| FIRE4 | GPIO11 | output | output-tested OK | Active-LOW output test passed |
| FIRE5 | GPIO12 | output | output-tested OK | Active-LOW output test passed |
| FIRE6 | GPIO13 | output | output-tested OK | Active-LOW output test passed |
| FIRE7 | GPIO14 | output | output-tested OK | Active-LOW output test passed |
| FIRE8 | GPIO21 | output | output-tested OK | Active-LOW output test passed |
| FIRE9 / Big Poof | GPIO47 | output | output-tested OK | Active-LOW output test passed |

## 6. Current Temporary Button Allocation

Logical button mapping:

```text
Button 1 = BUTTON_PINS[0] = GPIO4
Button 2 = BUTTON_PINS[1] = GPIO5
Button 3 = BUTTON_PINS[2] = GPIO6
Button 4 = BUTTON_PINS[3] = GPIO7
Button 5 = BUTTON_PINS[4] = GPIO15
Button 6 = BUTTON_PINS[5] = GPIO16
Button 7 = BUTTON_PINS[6] = GPIO17
Button 8 = BUTTON_PINS[7] = GPIO18
```

Current code map:

```cpp
const int BUTTON_PINS[8] = {
  4,   // Button 1
  5,   // Button 2
  6,   // Button 3
  7,   // Button 4
  15,  // Button 5
  16,  // Button 6
  17,  // Button 7
  18   // Button 8
};
```

Button test result:

```text
All button input pins read LOW when unconnected.
All button input pins read HIGH when connected to 3V3.
Internal pull-down mode worked for bench testing.
```

## 7. Button Input Pin Contract

Buttons use active-HIGH logic:

```text
GPIO HIGH = pressed / active
GPIO LOW  = released / inactive
```

The ESP32 sends 3.3V logic voltage out to the button panel.

When a button is pressed, that button returns 3.3V HIGH to its assigned ESP32 GPIO input.

The ESP32 GPIO input must not receive 5V.

The preferred input architecture does not use input relays between the buttons and ESP32 GPIO inputs.

The interaction logic must not invert the button behavior.

Pull-down wiring only defines the released/inactive state.

It does not change the active-HIGH button logic.

## 8. Input Pull-Down Contract

Each ESP32 button input signal line has a pull-down path to GND.

Input behavior:

```text
button released = 0V LOW
button pressed  = 3.3V HIGH
```

A released button is LOW because the input line is pulled down to GND.

Without a pull-down, a released button/input line can leave the ESP32 GPIO floating.

The final California wiring should use external 10kΩ pull-downs on the ESP32 button input lines.

This may be implemented with individual 10kΩ resistors or with a 10kΩ resistor array.

The important rule is:

```text
one pull-down path from each button input signal line to GND
```

Preferred direct button wiring:

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

## 9. India Bench Pull-Down Mode

For India bench testing, the firmware uses the ESP32 internal pull-downs:

```cpp
pinMode(BUTTON_PINS[i], INPUT_PULLDOWN);
```

This allows simple test wiring:

```text
3.3V -> button/switch -> ESP32 GPIO input
```

When the button/switch is open, the ESP32 internal pull-down holds the input LOW.

When the button/switch is closed, the input receives 3.3V HIGH.

## 10. California Final Pull-Down Mode

For final California hardware, the firmware should use normal input mode:

```cpp
pinMode(BUTTON_PINS[i], INPUT);
```

External 10kΩ pull-downs hold each button return input LOW when the matching button is released.

When the matching button is pressed, the button return wire sends 3.3V HIGH to the ESP32 GPIO input.

The firmware keeps the pull-down mode selectable through a configuration flag.

Changing between internal and external pull-down mode changes only the pin setup mode.

It does not change the logical behavior:

```text
LOW  = inactive
HIGH = active
```

## 11. Current Temporary FIRE Allocation

Logical FIRE output mapping:

```text
FIRE1 = FIRE_PINS[0] = GPIO8
FIRE2 = FIRE_PINS[1] = GPIO9
FIRE3 = FIRE_PINS[2] = GPIO10
FIRE4 = FIRE_PINS[3] = GPIO11
FIRE5 = FIRE_PINS[4] = GPIO12
FIRE6 = FIRE_PINS[5] = GPIO13
FIRE7 = FIRE_PINS[6] = GPIO14
FIRE8 = FIRE_PINS[7] = GPIO21
FIRE9 = FIRE_PINS[8] = GPIO47
```

Current code map:

```cpp
const int FIRE_PINS[9] = {
  8,   // FIRE1
  9,   // FIRE2
  10,  // FIRE3
  11,  // FIRE4
  12,  // FIRE5
  13,  // FIRE6
  14,  // FIRE7
  21,  // FIRE8
  47   // FIRE9 / Big Poof
};
```

Current FIRE output status:

```text
All FIRE pins are physically mapped.
All FIRE pins passed input-HIGH mapping test.
All FIRE pins passed active-LOW LED output testing.
FIRE outputs use active-LOW relay logic.
Idle/safe state = HIGH.
Triggered/pulse state = LOW.
```

Latest real firmware mapping test result:

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



```
Big Poof behavior:

```text
Button 1 alone = FIRE9 not triggered
Button 8 alone = FIRE9 not triggered
Button 1 + Button 8 = FIRE9 / Big Poof pulse triggered
FIRE9 uses the same active-LOW output behavior: HIGH idle, LOW trigger, HIGH return
```

## 12. FIRE Output Pin Contract

FIRE1-FIRE8 are normal FIRE outputs.

FIRE9 is reserved for the big poof output.

A FIRE output is an ESP32-controlled logic signal intended to trigger a relay channel for a poofer/solenoid circuit.

Poofer relay outputs use active-LOW logic:

```text
GPIO HIGH = relay inactive
GPIO LOW  = relay triggered
```

All FIRE output pins should initialize HIGH during startup.

During a FIRE pulse, the ESP32 pulls the relevant FIRE output LOW.

After the pulse duration ends, the ESP32 returns the FIRE output HIGH.

The ESP32 does not directly power the solenoid.

The FIRE output pins are independent from the button input simplification work.

The FIRE output pins are also independent from the LED Output Expander work.

Firmware should define:

```cpp
const int FIRE_IDLE_LEVEL = HIGH;
const int FIRE_TRIGGER_LEVEL = LOW;
```

## 13. Active-LOW FIRE LED Test Wiring

For safe active-LOW LED testing, the bench wiring is:

```text
3V3 -> resistor -> LED long leg
LED short leg -> ESP32 FIRE GPIO
```

Expected result:

```text
GPIO HIGH = LED off
GPIO LOW  = LED on
```

This matches the relay logic:

```text
HIGH = idle / relay inactive
LOW  = trigger / relay active
```

Do not wire this active-LOW LED test as if the ESP32 GPIO is sourcing the LED current.

The GPIO is sinking current during the LOW trigger state.

## 14. OLED Pin Contract

The OLED is a small SSD1306 diagnostic display.

Confirmed OLED:

```text
0.96 inch
128x64
SSD1306
I2C
```

Current OLED allocation:

```text
OLED SDA = GPIO1
OLED SCL = GPIO2
OLED VCC = 3V3
OLED GND = GND
```

Current code map:

```cpp
const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 2;
```

OLED test displayed:

```text
Tardi Controller
OLED test OK

SDA = GPIO1
SCL = GPIO2
```

OLED pins must stay reserved unless the pin map is deliberately changed and retested.

## 15. LED Output Architecture

Current LED output architecture assumes:

```text
ESP32-S3 -> UART -> standard Pixelblaze Output Expander v3.0 -> LED zones
```

The ESP32-S3 owns:

```text
controller logic
button interpretation
FIRE output logic
LED animation logic
```

The Output Expander is used as:

```text
8-channel 5V level-shifted LED output board
```

The Output Expander does not power the LED strips.

The Output Expander does not replace LED power injection.

LED strip power remains separate from ESP32 power.

See:

```text
docs/led_output_expander.md
docs/led_animation_architecture.md
```

## 16. LED Output Expander Pin Contract

The Output Expander connection should be treated as a UART LED data connection, not as eight separate ESP32 GPIO outputs.

Current intended path:

```text
ESP32-S3 UART output
 -> Pixelblaze Output Expander v3.0 data input
 -> 8 LED output channels
 -> LED zones
```

The ESP32-S3 should not use one normal GPIO per LED zone for this architecture.

Instead, the firmware should centralize the UART configuration for the Output Expander.

Current planned Output Expander UART settings:

```text
TX pin = GPIO39
baud   = 2000000
```

Before using the real UART output path, confirm:

```text
which ESP32-S3 UART peripheral will be used
whether UART RX is needed
whether any enable/sync/control pin is needed
which ground/reference connection is required
whether GPIO39 is still free from conflicts with USB Serial, OLED, buttons, FIRE outputs, or boot strapping pins
```

Do not use board-labelled TX/RX / UART0 for the Output Expander.
Do not use GPIO16 because it is Button 6.
Do not use PBDriverAdapter's old default ESP32 TX GPIO23 for Tardi real output.

Do not reuse these already assigned project pins for the Output Expander unless the pin map is deliberately revised:

```text
GPIO1  = OLED SDA
GPIO2  = OLED SCL
GPIO4  = Button 1
GPIO5  = Button 2
GPIO6  = Button 3
GPIO7  = Button 4
GPIO15 = Button 5
GPIO16 = Button 6
GPIO17 = Button 7
GPIO18 = Button 8
GPIO8  = FIRE1
GPIO9  = FIRE2
GPIO10 = FIRE3
GPIO11 = FIRE4
GPIO12 = FIRE5
GPIO13 = FIRE6
GPIO14 = FIRE7
GPIO21 = FIRE8
GPIO47 = FIRE9 / Big Poof
```

Likely UART candidates should come from the confirmed available / unassigned GPIO pool, not from the current button, FIRE, or OLED pins.

## 17. Available / Unassigned GPIOs

These GPIOs passed the same LOW/HIGH input mapping test, but are not currently assigned to core controller functions.

| GPIO | Current status | Notes |
|---:|---|---|
| GPIO3 | bench-tested OK / caution | Strapping-related caution; leave unassigned unless deliberately needed |
| GPIO19 | bench-tested OK / caution | Leave unassigned for now |
| GPIO20 | bench-tested OK / caution | Leave unassigned for now |
| GPIO35 | bench-tested OK / caution | Leave unassigned for now |
| GPIO36 | bench-tested OK / caution | Leave unassigned for now |
| GPIO37 | bench-tested OK / caution | Leave unassigned for now |
| GPIO38 | bench-tested OK / caution | Leave unassigned for now |
| GPIO39 | bench-tested OK / planned LED UART TX | Planned Pixelblaze Output Expander UART TX |
| GPIO40 | bench-tested OK / available candidate | Possible future LED UART / diagnostic pin |
| GPIO41 | bench-tested OK / available candidate | Possible future LED UART / diagnostic pin |
| GPIO45 | bench-tested OK / caution | Strapping-related caution; leave unassigned unless deliberately needed |
| GPIO46 | bench-tested OK / caution | Strapping-related caution; leave unassigned unless deliberately needed |

Current planned LED UART TX:

GPIO39

Other candidates to keep available for diagnostics or fallback only:

GPIO40
GPIO41

Caution pool:

```text
GPIO3
GPIO19
GPIO20
GPIO35
GPIO36
GPIO37
GPIO38
GPIO45
GPIO46
```

The caution GPIOs are not automatically forbidden, but they should not be used casually.

## 18. Avoid / Reserved Pins

| Pin | Status | Reason |
|---|---|---|
| GPIO0 | avoid | Read HIGH by itself during bench input test; boot-related behaviour suspected |
| GPIO48 | avoid / caution | Likely onboard RGB/status LED related; unexpected onboard light behaviour observed |
| TX/RX | reserved | Serial/programming related |
| 5V | avoid | Not for GPIO/OLED tests |
| GPIO1 | reserved | Used for OLED SDA |
| GPIO2 | reserved | Used for OLED SCL |
| GND | power reference | Ground only |
| 3V3 | power | 3.3V power only |
| RST | reset | Reset pin, not general GPIO |

Do not assign avoided pins to button inputs, FIRE outputs, OLED, or LED Output Expander UART without deliberate review and retesting.

## 19. Logical LED Zone Mapping

The controller should use neutral LED zone wording.

Logical LED mapping:

```text
LED zone 1 = LED response for Button 1 / FIRE1
LED zone 2 = LED response for Button 2 / FIRE2
LED zone 3 = LED response for Button 3 / FIRE3
LED zone 4 = LED response for Button 4 / FIRE4
LED zone 5 = LED response for Button 5 / FIRE5
LED zone 6 = LED response for Button 6 / FIRE6
LED zone 7 = LED response for Button 7 / FIRE7
LED zone 8 = LED response for Button 8 / FIRE8
LED BIG    = big-poof LED response / effect
```

The OLED and interaction logic should use neutral `LED` wording.

Use:

```text
LED: 4
LED: BIG
```

Do not use main-display wording like:

```text
Pixelblaze: 4
Output Expander: 4
```

The Output Expander is the hardware output path, not the user-facing interaction name.

## 20. DC Ground Requirement

The ESP32 must share the same DC ground reference as the button input signal circuit and the controller-side signal circuits.

A 3.3V HIGH signal is only valid when the ESP32 and the related controller-side signal circuits share a DC ground reference.

This applies to:

```text
button input signals
FIRE relay control signals
OLED module
LED Output Expander data/reference wiring
```

Do not confuse this with the high-power AC or high-current poofer/solenoid side shown separately in the inherited schematic.

High-current loads remain separate from ESP32 logic power.

## 21. Left-Side Physical Map

Current left-side map confirmed from board labels and bench testing.

| Board label | Current use | Bench result |
|---|---|---|
| 3V3 | 3.3V power | Used successfully |
| 3V3 | 3.3V power | Used successfully |
| RST | Reset | Not a GPIO |
| GPIO4 | Button 1 | Confirmed LOW/HIGH input test |
| GPIO5 | Button 2 | Confirmed LOW/HIGH input test |
| GPIO6 | Button 3 | Confirmed LOW/HIGH input test |
| GPIO7 | Button 4 | Confirmed LOW/HIGH input test |
| GPIO15 | Button 5 | Confirmed LOW/HIGH input test |
| GPIO16 | Button 6 | Confirmed LOW/HIGH input test |
| GPIO17 | Button 7 | Confirmed LOW/HIGH input test |
| GPIO18 | Button 8 | Confirmed LOW/HIGH input test |
| GPIO8 | FIRE1 | Active-LOW output test passed |
| GPIO3 | Unassigned | Confirmed LOW/HIGH input test; caution |
| GPIO46 | Unassigned | Confirmed LOW/HIGH input test; caution |
| GPIO9 | FIRE2 | Active-LOW output test passed |
| GPIO10 | FIRE3 | Active-LOW output test passed |
| GPIO11 | FIRE4 | Active-LOW output test passed |
| GPIO12 | FIRE5 | Active-LOW output test passed |
| GPIO13 | FIRE6 | Active-LOW output test passed |
| GPIO14 | FIRE7 | Active-LOW output test passed |
| 5V | Avoid | Do not use for GPIO/OLED tests |
| GND | Ground | Used successfully |

## 22. Right-Side Physical Map

Current right-side map confirmed from board labels and bench testing.

| Board label | Current use | Bench result |
|---|---|---|
| GND | Ground | Used successfully |
| TX | Reserved | Serial/programming related |
| RX | Reserved | Serial/programming related |
| GPIO1 | OLED SDA | Confirmed OLED working |
| GPIO2 | OLED SCL | Confirmed OLED working |
| GPIO42 | Unassigned | Not tested yet |
| GPIO41 | Unassigned / available candidate | Confirmed LOW/HIGH input test |
| GPIO40 | Unassigned / available candidate | Confirmed LOW/HIGH input test |
| GPIO39 | Planned LED UART TX | Confirmed LOW/HIGH input test |
| GPIO38 | Unassigned / caution | Confirmed LOW/HIGH input test |
| GPIO37 | Unassigned / caution | Confirmed LOW/HIGH input test |
| GPIO36 | Unassigned / caution | Confirmed LOW/HIGH input test |
| GPIO35 | Unassigned / caution | Confirmed LOW/HIGH input test |
| GPIO0 | Avoid | Reads HIGH by itself |
| GPIO45 | Unassigned / caution | Confirmed LOW/HIGH input test |
| GPIO48 | Avoid / caution | Likely onboard RGB/status LED related |
| GPIO47 | FIRE9 / Big Poof | Active-LOW output test passed |
| GPIO21 | FIRE8 | Active-LOW output test passed |
| GPIO20 | Unassigned / caution | Confirmed LOW/HIGH input test |
| GPIO19 | Unassigned / caution | Confirmed LOW/HIGH input test |
| GND | Ground | Used successfully |
| GND | Ground | Used successfully |

## 23. Code Pin Map

Use this concise map near the top of the controller sketch.

```cpp
// =====================================================
// ESP32-S3-DevKitC-1-N8R8 BOARD MAP
// Module: ESP32-S3-WROOM-1
// Arduino board option: ESP32S3 Dev Module
//
// Bench mapping method:
// GPIOs were set as INPUT_PULLDOWN.
// Each GPIO should read LOW normally.
// Each GPIO should read HIGH when touched/connected to 3V3.
//
// FIRE relay logic:
// HIGH = idle / relay inactive
// LOW  = trigger / relay active
// =====================================================
//
// CONFIRMED PROJECT PINS
//
// | Function        | GPIO | Status                         |
// |-----------------|------|--------------------------------|
// | OLED SDA        | 1    | Confirmed working              |
// | OLED SCL        | 2    | Confirmed working              |
// | Button 1        | 4    | Confirmed LOW/HIGH input test  |
// | Button 2        | 5    | Confirmed LOW/HIGH input test  |
// | Button 3        | 6    | Confirmed LOW/HIGH input test  |
// | Button 4        | 7    | Confirmed LOW/HIGH input test  |
// | Button 5        | 15   | Confirmed LOW/HIGH input test  |
// | Button 6        | 16   | Confirmed LOW/HIGH input test  |
// | Button 7        | 17   | Confirmed LOW/HIGH input test  |
// | Button 8        | 18   | Confirmed LOW/HIGH input test  |
// | FIRE1           | 8    | Active-LOW output test passed  |
// | FIRE2           | 9    | Active-LOW output test passed  |
// | FIRE3           | 10   | Active-LOW output test passed  |
// | FIRE4           | 11   | Active-LOW output test passed  |
// | FIRE5           | 12   | Active-LOW output test passed  |
// | FIRE6           | 13   | Active-LOW output test passed  |
// | FIRE7           | 14   | Active-LOW output test passed  |
// | FIRE8           | 21   | Active-LOW output test passed  |
// | FIRE9 Big Poof  | 47   | Active-LOW output test passed  |
//
// AVAILABLE / UNASSIGNED CANDIDATES
//
// | GPIO | Status                                  |
// |------|-----------------------------------------|
// | 39   | Available candidate / tested LOW-HIGH   |
// | 40   | Available candidate / tested LOW-HIGH   |
// | 41   | Available candidate / tested LOW-HIGH   |
//
// CAUTION / LEAVE UNASSIGNED UNLESS NEEDED
//
// | GPIO | Status                                  |
// |------|-----------------------------------------|
// | 3    | Caution / tested LOW-HIGH               |
// | 19   | Caution / tested LOW-HIGH               |
// | 20   | Caution / tested LOW-HIGH               |
// | 35   | Caution / tested LOW-HIGH               |
// | 36   | Caution / tested LOW-HIGH               |
// | 37   | Caution / tested LOW-HIGH               |
// | 38   | Caution / tested LOW-HIGH               |
// | 45   | Caution / tested LOW-HIGH               |
// | 46   | Caution / tested LOW-HIGH               |
//
// AVOID / RESERVED
//
// | Pin    | Reason                                                  |
// |--------|---------------------------------------------------------|
// | GPIO0  | Reads HIGH by itself / boot-related behaviour suspected |
// | GPIO48 | Likely onboard RGB/status LED related                   |
// | TX/RX  | Serial/programming related                              |
// | 5V     | Not for GPIO/OLED tests                                 |
// | GPIO1  | Reserved for OLED SDA                                   |
// | GPIO2  | Reserved for OLED SCL                                   |
// =====================================================

const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 2;

const int BUTTON_PINS[8] = {
  4,   // Button 1
  5,   // Button 2
  6,   // Button 3
  7,   // Button 4
  15,  // Button 5
  16,  // Button 6
  17,  // Button 7
  18   // Button 8
};

const int FIRE_PINS[9] = {
  8,   // FIRE1
  9,   // FIRE2
  10,  // FIRE3
  11,  // FIRE4
  12,  // FIRE5
  13,  // FIRE6
  14,  // FIRE7
  21,  // FIRE8
  47   // FIRE9 / Big Poof
};

const int FIRE_IDLE_LEVEL = HIGH;
const int FIRE_TRIGGER_LEVEL = LOW;

const int FIRST_CANDIDATE_GPIO_PINS[] = {
  39, 40, 41
};
```

## 24. Why Pins Are Centralized

The India and California builds use the same ESP32-S3 board family.

Even so, pin numbers remain centralized because:

```text
wiring can change
a board can be replaced later
GPIO choices can need adjustment during testing
OLED pins must be reserved
UART pins for the LED Output Expander must be reserved
boot strapping pins and special startup pins must be avoided unless deliberately confirmed safe
interaction logic must not change just because physical pins change
```

## 25. Physical Mapping Later

This file does not yet define final physical sculpture positions.

Later, the confirmed physical map can be added here or in a separate physical mapping file:

```text
Button 1 = confirmed physical input position
Button 2 = confirmed physical input position
Button 3 = confirmed physical input position
Button 4 = confirmed physical input position
Button 5 = confirmed physical input position
Button 6 = confirmed physical input position
Button 7 = confirmed physical input position
Button 8 = confirmed physical input position

FIRE1 = confirmed physical poofer position
FIRE2 = confirmed physical poofer position
FIRE3 = confirmed physical poofer position
FIRE4 = confirmed physical poofer position
FIRE5 = confirmed physical poofer position
FIRE6 = confirmed physical poofer position
FIRE7 = confirmed physical poofer position
FIRE8 = confirmed physical poofer position
FIRE9 = big poof

LED zone 1 = confirmed LED zone / Output Expander channel
LED zone 2 = confirmed LED zone / Output Expander channel
LED zone 3 = confirmed LED zone / Output Expander channel
LED zone 4 = confirmed LED zone / Output Expander channel
LED zone 5 = confirmed LED zone / Output Expander channel
LED zone 6 = confirmed LED zone / Output Expander channel
LED zone 7 = confirmed LED zone / Output Expander channel
LED zone 8 = confirmed LED zone / Output Expander channel
LED BIG    = confirmed big-poof LED effect
```

## 26. Current Open Pin Questions

These still need confirmation:

```text
confirm GPIO39 works reliably as 2 Mbps UART TX to the Output Expander data input
Does the Output Expander need TX only, or TX plus any extra control/reference wiring?
Does the final California relay board trigger reliably from active-LOW ESP32 output behavior?
Will California wiring keep the current bench GPIO allocation or require reassignment?
How do LED zones 1-8 map to physical sculpture sections?
How many LEDs are on each Output Expander channel?
```
