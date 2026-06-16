# GPIO Schema

This file tracks the planned ESP32-S3 GPIO use for the Tardi Controller.

It records:

```text
logical use
physical GPIO number
direction
current status
reason
test status
notes
```

This file is a working schema.

It is not final California wiring until the pins have been bench-tested and confirmed against the final hardware.

## 1. Board Target

Current board:

```text
Espressif ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1
Arduino IDE board option: ESP32S3 Dev Module
Serial baud: 115200
```

Confirmed upload behaviour:

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

## 2. Bench Mapping Method

GPIOs were tested using internal pulldown mode:

```cpp
pinMode(TEST_PIN, INPUT_PULLDOWN);
```

Expected result:

```text
GPIO open / unconnected = LOW
GPIO touched or connected to 3V3 = HIGH
```

This confirms that a pin can behave correctly as a basic digital input.

Important distinction:

```text
Input-HIGH tested = pin reads LOW normally and HIGH when touched to 3V3.
Output-tested = pin has successfully driven a safe LED + resistor test load HIGH/LOW.
```

The FIRE pins have been physically mapped, input-HIGH tested, and output-tested.

All FIRE outputs passed active-LOW LED testing.

FIRE outputs use active-LOW relay logic:

```text
idle state = HIGH
triggered state = LOW
return state = HIGH
```

## 3. GPIO Status Labels

Use these status labels:

```text
use
temporary
reserved
avoid
caution
unknown
bench-tested OK
bench-tested failed
output-tested ok
```

Meaning:

```text
use                  = reasonable candidate for normal use
temporary            = currently used for bench testing, not final California wiring
reserved             = kept free for OLED, LED, PixelBlaze, UART, USB, or future use
avoid                = do not use for this project stage
caution              = possible use only after deliberate review/testing
unknown              = not assessed yet
bench-tested OK      = physically tested successfully on this board
bench-tested failed  = tested and found unsuitable
output-tested OK     = output verified with active-LOW LED test
```

## 4. Confirmed Project Pins

| Function | GPIO | Direction | Status | Bench result | Notes |
|---|---:|---|---|---|---|
| OLED SDA | GPIO1 | I2C | bench-tested OK | OLED confirmed working | Reserved for OLED SDA |
| OLED SCL | GPIO2 | I2C | bench-tested OK | OLED confirmed working | Reserved for OLED SCL |
| Button 1 | GPIO4 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| Button 2 | GPIO5 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| Button 3 | GPIO6 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| Button 4 | GPIO7 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| Button 5 | GPIO15 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| Button 6 | GPIO16 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| Button 7 | GPIO17 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| Button 8 | GPIO18 | input | bench-tested OK | LOW idle, HIGH when connected to 3V3 | Current button input |
| FIRE1 | GPIO8 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE2 | GPIO9 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE3 | GPIO10 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE4 | GPIO11 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE5 | GPIO12 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE6 | GPIO13 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE7 | GPIO14 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE8 | GPIO21 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |
| FIRE9 / Big Poof | GPIO47 | output | output-tested OK | Active-LOW LED test passed | HIGH idle, LOW trigger, HIGH return confirmed |

## 5. Confirmed Available / Unassigned GPIOs

These GPIOs passed the same LOW/HIGH input mapping test, but are not currently assigned to core controller functions.

| GPIO | Status | Bench result | Notes |
|---:|---|---|---|
| GPIO3 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Strapping-related caution; leave unassigned unless deliberately needed |
| GPIO19 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Avoid for Output Expander TX because of USB-related caveats |
| GPIO20 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Avoid for Output Expander TX because of USB-related caveats |
| GPIO35 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Avoid for N8R8/octal PSRAM board path |
| GPIO36 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Avoid for N8R8/octal PSRAM board path |
| GPIO37 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Avoid for N8R8/octal PSRAM board path |
| GPIO38 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Only investigate as Output Expander TX fallback if GPIO39 issue appears JTAG-related |
| GPIO39 | bench-tested OK / planned LED UART TX | LOW idle, HIGH when connected to 3V3 | Planned Pixelblaze Output Expander UART TX; JTAG-related caveat requires physical validation |
| GPIO40 | bench-tested OK / available candidate | LOW idle, HIGH when connected to 3V3 | First fallback if GPIO39 physically fails |
| GPIO41 | bench-tested OK / available candidate | LOW idle, HIGH when connected to 3V3 | Second fallback if GPIO39 physically fails |
| GPIO45 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Strapping-related caution; leave unassigned unless deliberately needed |
| GPIO46 | bench-tested OK / caution | LOW idle, HIGH when connected to 3V3 | Strapping-related caution; leave unassigned unless deliberately needed |

## 6. Avoid / Reserved Pins

| Pin | Status | Reason |
|---|---|---|
| GPIO0 | avoid | Read HIGH by itself during bench input test; boot-related behaviour suspected |
| GPIO48 | avoid / caution | Likely onboard RGB/status LED related; unexpected green/white onboard light behaviour observed |
| TX/RX | reserved | Serial/programming related |
| 5V | avoid | Not for GPIO/OLED tests |
| GPIO1 | reserved | Used for OLED SDA |
| GPIO2 | reserved | Used for OLED SCL |
| GND | power reference | Ground only |
| 3V3 | power | 3.3V power only |
| RST | reset | Reset pin, not general GPIO |

## 7. Current Temporary Controller Allocation

### Button Inputs

```text
Button 1 = GPIO4
Button 2 = GPIO5
Button 3 = GPIO6
Button 4 = GPIO7
Button 5 = GPIO15
Button 6 = GPIO16
Button 7 = GPIO17
Button 8 = GPIO18
```

Test result:

```text
All button input pins read LOW when unconnected.
All button input pins read HIGH when connected to 3V3.
Internal pulldown mode worked for bench testing.
```

### FIRE Outputs

```text
FIRE1 = GPIO8
FIRE2 = GPIO9
FIRE3 = GPIO10
FIRE4 = GPIO11
FIRE5 = GPIO12
FIRE6 = GPIO13
FIRE7 = GPIO14
FIRE8 = GPIO21
FIRE9 = GPIO47
```

Current status:

```text
All FIRE pins are physically mapped.
All FIRE pins passed input-HIGH mapping test.
All FIRE pins passed active-LOW LED output testing.
FIRE outputs use active-LOW relay logic.
Idle/safe state = HIGH.
Triggered/pulse state = LOW.
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

This confirms the current controller firmware maps the tested button inputs to the intended FIRE outputs.

```text
FIRE1-FIRE9 output-tested OK.

HIGH = idle / relay inactive
LOW = trigger / relay active

Verified on:
GPIO8
GPIO9
GPIO10
GPIO11
GPIO12
GPIO13
GPIO14
GPIO21
GPIO47
```

### OLED

```text
OLED SDA = GPIO1
OLED SCL = GPIO2
OLED VCC = 3V3
OLED GND = GND
```

Wire colours used during bench test:

```text
OLED GND = brown
OLED VCC = red
OLED SCL = yellow
OLED SDA = green
```

OLED test displayed:

```text
Tardi Controller
OLED test OK

SDA = GPIO1
SCL = GPIO2
```

## 8. Left-Side Physical Map

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
| GPIO9 | FIRE2 | Active-LOW output test passed  |
| GPIO10 | FIRE3 | Active-LOW output test passed  |
| GPIO11 | FIRE4 | Active-LOW output test passed  |
| GPIO12 | FIRE5 | Active-LOW output test passed  |
| GPIO13 | FIRE6 | Active-LOW output test passed  |
| GPIO14 | FIRE7 | Active-LOW output test passed  |
| 5V | Avoid | Do not use for GPIO/OLED tests |
| GND | Ground | Used successfully |

## 9. Right-Side Physical Map

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

## 10. Code Pin Map

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
// =====================================================
//
// CONFIRMED PROJECT PINS
//
// | Function        | GPIO | Status                                      |
// |-----------------|------|---------------------------------------------|
// | OLED SDA        | 1    | Confirmed working                           |
// | OLED SCL        | 2    | Confirmed working                           |
// | Button 1        | 4    | Confirmed LOW/HIGH input test               |
// | Button 2        | 5    | Confirmed LOW/HIGH input test               |
// | Button 3        | 6    | Confirmed LOW/HIGH input test               |
// | Button 4        | 7    | Confirmed LOW/HIGH input test               |
// | Button 5        | 15   | Confirmed LOW/HIGH input test               |
// | Button 6        | 16   | Confirmed LOW/HIGH input test               |
// | Button 7        | 17   | Confirmed LOW/HIGH input test               |
// | Button 8        | 18   | Confirmed LOW/HIGH input test               |
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
// CONFIRMED AVAILABLE / UNASSIGNED GPIOs
//
// | GPIO | Status                    |
// |------|---------------------------|
// | 3    | Confirmed LOW/HIGH test   |
// | 19   | Confirmed LOW/HIGH test   |
// | 20   | Confirmed LOW/HIGH test   |
// | 35   | Confirmed LOW/HIGH test   |
// | 36   | Confirmed LOW/HIGH test   |
// | 37   | Confirmed LOW/HIGH test   |
// | 38   | Confirmed LOW/HIGH test   |
// | 39   | Confirmed LOW/HIGH test; planned Output Expander UART TX |
// | 40   | Confirmed LOW/HIGH test   |
// | 41   | Confirmed LOW/HIGH test   |
// | 45   | Confirmed LOW/HIGH test   |
// | 46   | Confirmed LOW/HIGH test   |
//
// AVOID / RESERVED
//
// | Pin   | Reason                                                   |
// |-------|----------------------------------------------------------|
// | GPIO0 | Reads HIGH by itself / boot-related behaviour suspected |
// | GPIO48| Likely onboard RGB/status LED related                   |
// | TX/RX | Serial/programming related                              |
// | 5V    | Not for GPIO/OLED tests                                  |
// | GPIO1 | Reserved for OLED SDA                                    |
// | GPIO2 | Reserved for OLED SCL                                    |
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

const int AVAILABLE_GPIO_PINS[] = {
  3, 19, 20, 35, 36, 37, 38, 39, 40, 41, 45, 46
};
```

## 11. Rule For Firmware

The firmware should keep raw GPIO numbers centralized.

Use arrays:

```cpp
const int BUTTON_PINS[NUM_BUTTONS] = {
  4, 5, 6, 7, 15, 16, 17, 18
};

const int FIRE_PINS[NUM_FIRE_OUTPUTS] = {
  8, 9, 10, 11, 12, 13, 14, 21, 47
};
```

OLED pins should also remain centralized:

```cpp
const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 2;
```

Do not scatter raw GPIO numbers through the interaction logic.

## 12. Test Priorities

Latest completed test:

Current controller firmware mapping test PASSED.

Test order:

```text
1. ESP32 boots with current firmware.
2. Serial Monitor works.
3. OLED works on GPIO1/GPIO2. COMPLETE.
4. Button input pins read correctly. COMPLETE.
5. Additional GPIO mapping test. COMPLETE for listed pins.
6. FIRE1-FIRE9 active-LOW LED output test. COMPLETE.
7. GPIO47 confirmed as reliable FIRE9 output. COMPLETE.
8. GPIO39 is planned for Pixelblaze Output Expander UART TX; keep GPIO16 as Button 6.
```

Do not connect output pins to relay boards, solenoids, poofers, gas, flame hardware, or LED strips during this bench stage.

## 13. Current Open Questions

These still need confirmation:

```text
GPIO39 2 Mbps UART electrical validation with the Pixelblaze Output Expander still needs California-side testing.
Do the final California relay boards trigger reliably when the ESP32 output is pulled LOW?
```
