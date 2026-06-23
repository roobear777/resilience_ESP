````markdown
# GPIO Test Plan

This file defines the GPIO bench-test plan for the Tardi Controller ESP32-S3 board.

It is used together with:

```text
docs/gpio_schema.md
docs/pin_mapping.md
firmware/gpio_test/gpio_test.ino
firmware/esp32_controller/esp32_controller.ino
```

The goal is to confirm which planned GPIOs work safely and reliably before the pin map is treated as bench-tested.

This file tests ESP32 GPIO behavior only.

It does not prove that final relays, solenoids, LEDs, the Pixelblaze Output Expander, or live fire hardware work.

## 1. Test Purpose

The GPIO tests should confirm:

```text
the ESP32 boots normally
USB upload still works
Serial Monitor still works
OLED I2C pins work
button input pins read LOW when inactive
button input pins read HIGH when connected to 3.3V
FIRE output pins idle HIGH, pulse LOW, and return HIGH safely
FIRE output pins use active-LOW relay logic
GPIO47 works reliably as FIRE9 / Big Poof output
current GPIO map matches docs/gpio_schema.md
avoided pins are not used
candidate UART pins for the LED Output Expander remain available
```

These tests do not prove that final relays, solenoids, LEDs, the Output Expander, or live fire hardware work.

They only prove the ESP32 GPIO choices are reasonable for the controller firmware.

## 2. Board Under Test

Current board:

```text
Espressif ESP32-S3-DevKitC-1-N8R8
Module: ESP32-S3-WROOM-1-N8R8
Flash: 8 MB
PSRAM: 8 MB
Arduino IDE board option: ESP32S3 Dev Module
Serial baud: 115200
```

The GPIO schema is based on this board/module combination.

Do not assume the same GPIO safety rules apply to a different ESP32-S3 board.

## 3. Test Safety Rules

Do not connect any of these during GPIO testing:

```text
poofer hardware
solenoids
gas hardware
flame hardware
high-current loads
relay boards unless explicitly testing relays later
LED strips
Pixelblaze Output Expander
external power supplies for fire hardware
external power supplies for LED loads
```

Use only safe bench items:

```text
USB cable
breadboard
jumper wires
pushbuttons or simple jumper leads
small LEDs with resistors
multimeter if available
OLED module
```

Do not test avoided pins.

Do not connect any ESP32 GPIO to 5V.

ESP32 GPIO inputs must receive 3.3V logic only.

Do not connect output pins to relay boards, solenoids, poofers, gas, flame hardware, or LED strips during this GPIO bench-test stage.

## 4. Planned GPIOs To Test

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
FIRE9 / Big Poof = GPIO47
```

### OLED I2C

```text
OLED SDA = GPIO1
OLED SCL = GPIO2
```

### First Candidate Pins For Future LED Output Expander UART / Diagnostics

These are reserved for later and should not be assigned until the core input/output pins are stable:

```text
GPIO39
GPIO40
GPIO41
```

These are candidates only.

They are not yet confirmed as UART pins for the Pixelblaze Output Expander.

Additional mapped-but-unassigned GPIOs exist, but some carry caution notes and should stay unassigned unless deliberately needed.

## 5. Pins Not To Test As Normal Project GPIOs

Do not use these as normal project GPIOs:

```text
GPIO0  = avoid; reads HIGH by itself during bench input test; boot-related behaviour suspected
GPIO48 = avoid / caution; likely onboard RGB/status LED related
TX/RX  = reserved for Serial/programming
5V     = not for GPIO/OLED tests
GPIO1  = reserved for OLED SDA
GPIO2  = reserved for OLED SCL
RST    = reset pin, not general GPIO
```

Caution GPIOs should stay unassigned unless deliberately needed:

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

## 6. Test Stages

Testing should happen in this order:

```text
Stage 1: Boot and upload test
Stage 2: Serial Monitor test
Stage 3: OLED I2C test
Stage 4: Button input GPIO test
Stage 5: FIRE output GPIO active-LOW test
Stage 6: GPIO47 FIRE9 specific test
Stage 7: One-shot firmware GPIO test
Stage 8: Big Poof GPIO test
Stage 9: Candidate GPIO reservation check
Stage 10: Firmware pin map comparison
Stage 11: Update gpio_schema.md
```

Do not skip ahead.

The LED Output Expander UART test is not part of this GPIO test plan.

That belongs in:

```text
docs/test_plan.md
docs/led_output_expander.md
```

## 7. Stage 1: Boot and Upload Test

Purpose:

```text
Confirm the ESP32 boots and accepts firmware uploads.
```

Setup:

```text
ESP32 connected by USB
no external wiring attached
Arduino IDE open
correct board selected
correct port selected
```

Test:

```text
Upload a simple known-working sketch.
Confirm upload completes.
Confirm ESP32 restarts.
```

Pass condition:

```text
Upload succeeds.
Board restarts normally.
No manual boot/reset workaround is needed beyond normal upload behavior.
```

Known acceptable upload behavior:

```text
If upload pauses at Connecting..., hold BOOT until writing starts, then release.
After upload finishes, press RESET if needed.
```

Fail condition:

```text
Upload fails.
Board does not reconnect.
Serial port disappears unexpectedly.
Board gets stuck in boot/download mode.
```

Record:

```text
date
Arduino IDE board selection
port name
upload result
any reset/boot button behavior required
```

## 8. Stage 2: Serial Monitor Test

Purpose:

```text
Confirm Serial Monitor works before testing GPIOs.
```

Setup:

```text
ESP32 connected by USB
Serial Monitor baud rate 115200
no external wiring attached
```

Test:

```text
Upload a sketch that prints a startup message and repeating tick.
Open Serial Monitor.
Confirm readable output.
```

Expected output example:

```text
GPIO test starting...
tick
tick
tick
```

Pass condition:

```text
Serial output appears clearly at 115200 baud.
Serial remains stable.
```

Fail condition:

```text
No Serial output.
Unreadable output.
Board resets repeatedly.
Serial port disconnects.
```

## 9. Stage 3: OLED I2C Test

Purpose:

```text
Confirm the OLED works on the planned I2C pins.
```

Planned OLED pins:

```text
OLED SDA = GPIO1
OLED SCL = GPIO2
```

Expected OLED:

```text
SSD1306
128x64
I2C
likely address 0x3C
```

Setup:

```text
OLED GND -> ESP32 GND
OLED VCC -> ESP32 3V3
OLED SDA -> GPIO1
OLED SCL -> GPIO2
```

Test:

```text
Upload OLED test sketch.
Initialize display at address 0x3C.
Show simple text.
```

Expected display:

```text
OLED TEST
GPIO1 SDA
GPIO2 SCL
```

Pass condition:

```text
OLED displays text correctly.
Serial reports OLED initialized.
ESP32 still uploads normally afterward.
```

Fail condition:

```text
OLED does not initialize.
OLED stays blank.
ESP32 boot/upload is affected.
I2C scan does not find display.
```

If OLED fails:

```text
check wiring
check OLED address
check voltage
check SDA/SCL order
try I2C scanner
do not change main pin map until wiring/address is checked
```

## 10. Stage 4: Button Input GPIO Test

Purpose:

```text
Confirm each planned input GPIO reads LOW when inactive and HIGH when connected to 3.3V.
```

Test pins:

```text
GPIO4
GPIO5
GPIO6
GPIO7
GPIO15
GPIO16
GPIO17
GPIO18
```

Setup:

```text
use internal pull-down mode
no external relay hardware
one jumper wire from 3.3V used to test each input
```

Input test wiring:

```text
input open = should read LOW
input connected to 3.3V = should read HIGH
```

For each input:

```text
1. Leave the GPIO unconnected.
2. Confirm Serial shows LOW.
3. Connect GPIO to 3.3V.
4. Confirm Serial shows HIGH.
5. Remove 3.3V.
6. Confirm Serial returns LOW.
```

Expected result for GPIO4 / Button 1:

```text
GPIO4 idle = LOW
GPIO4 connected to 3.3V = HIGH
GPIO4 released = LOW
```

Repeat for:

```text
GPIO5
GPIO6
GPIO7
GPIO15
GPIO16
GPIO17
GPIO18
```

Pass condition:

```text
each input reads LOW when open
each input reads HIGH when connected to 3.3V
each input returns LOW after disconnect
no other input changes unexpectedly
Serial output remains stable
```

Fail condition:

```text
input floats HIGH when open
input never goes HIGH
input does not return LOW
wrong input number changes
multiple inputs change from one jumper
board resets
USB/Serial becomes unstable
```

Record in `docs/gpio_schema.md`:

```text
bench-tested OK
```

only if the pin passes all input checks.

## 11. Stage 5: FIRE Output GPIO Active-LOW Test

Purpose:

```text
Confirm each planned FIRE output GPIO can idle HIGH, pulse LOW, and return HIGH safely.
```

Test pins:

```text
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

Setup:

```text
no relay board
no solenoid
no fire hardware
use Serial first
then use a current-limited LED or multimeter
```

FIRE outputs use active-LOW relay logic:

```text
HIGH = idle / relay inactive
LOW  = trigger / relay active
```

Safe active-LOW LED test wiring:

```text
3V3 -> resistor -> LED long leg
LED short leg -> ESP32 FIRE GPIO
```

Expected LED behavior:

```text
GPIO HIGH = LED off
GPIO LOW  = LED on
```

This matches the relay behavior:

```text
GPIO HIGH = idle / relay inactive
GPIO LOW  = trigger / relay active
```

Do not connect an LED directly without a resistor.

Do not wire this test as:

```text
ESP32 GPIO -> resistor -> LED -> GND
```

That wiring tests GPIO source behavior and does not match the active-LOW relay behavior we need to prove.

Test behavior:

```text
set output HIGH for idle/safe state
pulse output LOW briefly
return output HIGH
```

For each output:

```text
1. Confirm output starts HIGH.
2. Confirm LED is off in idle state.
3. Pulse output LOW.
4. Confirm LED turns on during trigger state.
5. Return output HIGH.
6. Confirm LED turns off again.
7. Repeat several times.
```

Pass condition:

```text
output starts HIGH
LED is off when output is HIGH
output pulses LOW when commanded
LED turns on only while output is LOW
output returns HIGH when commanded
LED turns off when output returns HIGH
no reset occurs
no upload/USB issue occurs
no other output changes unexpectedly
```

Fail condition:

```text
output cannot be controlled
output stays HIGH
output stays LOW
LED behavior is inverted from expectation
wrong pin changes
board resets
USB/Serial becomes unstable
```

Record in `docs/gpio_schema.md`:

```text
output-tested OK
```

only if the pin passes all output checks.

## 12. Stage 6: GPIO47 FIRE9 Specific Test

Purpose:

```text
Confirm GPIO47 is reliable enough for FIRE9 / Big Poof output.
```

Reason:

```text
GPIO47 is assigned to FIRE9 / Big Poof, so it needs extra confidence.
```

Setup:

```text
no relay board
no solenoid
no fire hardware
use active-LOW LED wiring or multimeter
```

Active-LOW LED wiring:

```text
3V3 -> resistor -> LED long leg
LED short leg -> GPIO47
```

Test:

```text
1. Upload GPIO test sketch using GPIO47 as output.
2. Confirm board boots normally.
3. Confirm Serial Monitor works.
4. Confirm GPIO47 starts HIGH.
5. Toggle GPIO47 LOW/HIGH at least 20 times.
6. Confirm LED turns on only when GPIO47 is LOW.
7. Confirm GPIO47 returns HIGH.
8. Re-upload firmware afterward to confirm upload still works.
```

Pass condition:

```text
GPIO47 toggles reliably
GPIO47 starts HIGH
GPIO47 pulses LOW
GPIO47 returns HIGH
ESP32 boots normally
ESP32 uploads normally afterward
Serial Monitor remains stable
```

Fail condition:

```text
GPIO47 does not toggle
GPIO47 affects boot
GPIO47 affects upload
GPIO47 affects Serial
GPIO47 behaves inconsistently
```

If GPIO47 fails:

```text
do not use it for FIRE9
move FIRE9 to another candidate GPIO
update gpio_schema.md
update pin_mapping.md
update esp32_controller.ino
```

## 13. Stage 7: One-Shot Firmware GPIO Test

Purpose:

```text
Confirm the real controller firmware drives selected outputs only during the intended 500 ms pulse.
```

Setup:

```text
main controller firmware loaded
simulation mode first
then live mode only with active-LOW LED or meter test load
no relay board
no solenoid
no fire hardware
```

Simulator mode expected OLED:

```text
SIMULATOR MODE

FIRING
Input: 4
Output: 4
LED: 4
No live output
```

Live mode expected OLED:

```text
LIVE MODE

FIRING
Input: 4
Output: 4
LED: 4
Live output: ON
```

Test:

```text
1. Press/test Input 4.
2. Confirm Output 4 activates for 500 ms in logic.
3. In live safe-output test, confirm FIRE4/GPIO11 goes LOW for 500 ms.
4. Keep Input 4 held.
5. Confirm output returns HIGH after pulse.
6. Release Input 4.
7. Press Input 4 again.
8. Confirm another 500 ms pulse occurs.
```

Pass condition:

```text
new press creates one output pulse
holding input does not keep output active
release and press again creates another pulse
output returns HIGH
OLED and Serial agree
```

Fail condition:

```text
holding input keeps output active
output never activates
output does not turn off
output does not return HIGH
wrong output activates
OLED and Serial disagree
```

## 14. Stage 8: Big Poof GPIO Test

Purpose:

```text
Confirm Input 1 + Input 8 triggers FIRE9 / GPIO47 behavior.
```

Setup:

```text
main controller firmware loaded
simulation mode first
live mode only with active-LOW LED or meter test load
no relay board
no solenoid
no fire hardware
```

Test:

```text
1. Press/test Input 1 and Input 8 together.
2. Confirm OLED shows BIG POOF.
3. Confirm Output shows 1 8 9.
4. In simulator mode, confirm No live output.
5. In live safe-output test, confirm GPIO47/FIRE9 goes LOW for 500 ms.
6. Confirm GPIO47/FIRE9 returns HIGH automatically.
```

Expected current behavior:

```text
Input: 1+8
Output: 1 8 9
LED: BIG
```

Current scaffold behavior:

```text
Button 1 + Button 8 can trigger FIRE1, FIRE8, and FIRE9.
```

Pass condition:

```text
FIRE9 triggers from Input 1 + Input 8
FIRE9 pulse ends automatically
FIRE9 returns HIGH
holding the combo does not keep FIRE9 active
Input 1 alone does not trigger FIRE9
Input 8 alone does not trigger FIRE9
```

Fail condition:

```text
FIRE9 does not trigger
FIRE9 stays active
FIRE9 does not return HIGH
FIRE9 triggers from Input 1 alone
FIRE9 triggers from Input 8 alone
```

## 15. Stage 9: Candidate GPIO Reservation Check

Do not run this stage until the core input and FIRE output pins are stable.

This stage does not confirm the Pixelblaze Output Expander.

It only checks whether likely spare GPIOs remain usable as ordinary outputs or future UART candidates.

First candidate pins:

```text
GPIO39
GPIO40
GPIO41
```

Purpose:

```text
Confirm candidate pins remain available for future UART output to the Pixelblaze Output Expander or diagnostics.
```

For each candidate:

```text
1. Confirm board boots with the pin unused.
2. Configure as output in a dedicated test sketch.
3. Toggle HIGH/LOW/HIGH.
4. Confirm no boot, upload, Serial, OLED, button, or FIRE problem.
5. Record result.
```

Pass condition:

```text
pin toggles reliably
no boot/upload issue
no Serial issue
no OLED issue
no button input issue
no FIRE output issue
```

Fail condition:

```text
pin affects boot/upload
pin conflicts with another controller function
pin behaves inconsistently
```

Do not assign GPIO39/GPIO40/GPIO41 to the Output Expander yet unless the UART wiring and firmware design are being tested deliberately.

## 16. Stage 10: Firmware Pin Map Comparison

Purpose:

```text
Confirm the firmware pin arrays match the GPIO schema.
```

Check:

```text
firmware/esp32_controller/esp32_controller.ino
```

Expected button array:

```cpp
const int BUTTON_PINS[NUM_BUTTONS] = {
  4, 5, 6, 7, 15, 16, 17, 18
};
```

Expected FIRE array:

```cpp
const int FIRE_PINS[NUM_FIRE_OUTPUTS] = {
  8, 9, 10, 11, 12, 13, 14, 21, 47
};
```

Expected OLED pins:

```cpp
const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 2;
```

Expected FIRE levels:

```cpp
const int FIRE_IDLE_LEVEL = HIGH;
const int FIRE_TRIGGER_LEVEL = LOW;
```

Pass condition:

```text
firmware arrays match gpio_schema.md
OLED pins are centralized
FIRE levels are centralized
no raw GPIO numbers are scattered through interaction logic
avoided pins are not used
candidate LED Output Expander UART pins remain unassigned unless deliberately added
```

Fail condition:

```text
firmware uses a pin not listed in schema
firmware uses avoided pin
firmware has active-HIGH FIRE assumptions
raw GPIO numbers are scattered through logic
docs and code disagree
```

## 17. Stage 11: Update gpio_schema.md

Purpose:

```text
Record test results in the GPIO schema.
```

After each test, update:

```text
docs/gpio_schema.md
```

Use this result format for inputs:

```text
Test status: bench-tested OK
Test result: reads LOW idle, HIGH on 3.3V
Notes: works with internal pull-down
```

Use this result format for FIRE outputs:

```text
Test status: output-tested OK
Test result: active-LOW output test passed
Notes: HIGH idle, LOW trigger, HIGH return confirmed
```

Use this result format for failures:

```text
Test status: bench-tested failed
Test result: output unstable
Notes: do not use; choose replacement GPIO
```

Record these details:

```text
date
board used
firmware/sketch used
GPIO tested
input/output mode
test wiring
result
notes
```

## 18. General Pass / Fail Rules

A GPIO passes only if:

```text
ESP32 boots normally
USB upload still works
Serial Monitor still works
pin behaves as expected
pin starts in safe state
pin returns to safe state
no other planned pin is affected
```

A button input passes only if:

```text
it reads LOW when open
it reads HIGH when connected to 3.3V
it returns LOW after disconnect
it does not cause other inputs to change
```

A FIRE output passes only if:

```text
it starts HIGH
it pulses LOW when commanded
it returns HIGH
active-LOW LED test behavior matches expectation
no other output changes unexpectedly
```

A GPIO fails if:

```text
boot becomes unreliable
upload becomes unreliable
Serial becomes unreliable
pin floats unexpectedly
pin cannot be controlled
pin affects another function
pin conflicts with OLED/USB/UART/flash/PSRAM/onboard RGB LED
```

## 19. Do Not Change Yet

Do not change these during GPIO testing unless a pin fails:

```text
button logic
debounce logic
500 ms pulse behavior
big-poof trigger logic
simulation/live mode behavior
OLED screen wording
LED animation architecture
Pixelblaze Output Expander architecture
```

GPIO testing is about validating the pin choices, not changing interaction behavior.


Replace it with:

````markdown
## 20. Completion Criteria

The GPIO test phase is complete for the current India bench-test controller mapping.

Completed results:

```text
all 8 button input pins are bench-tested OK
all 9 FIRE output pins are output-tested OK
GPIO47 is confirmed for FIRE9 / Big Poof
OLED works on GPIO1/GPIO2
avoided pins remain unused
candidate Output Expander UART pins remain reserved but not required
gpio_schema.md is updated with test results
esp32_controller.ino matches gpio_schema.md
Button 1 -> FIRE1 = PASS
Button 2 -> FIRE2 = PASS
Button 3 -> FIRE3 = PASS
Button 4 -> FIRE4 = PASS
Button 5 -> FIRE5 = PASS
Button 6 -> FIRE6 = PASS
Button 7 -> FIRE7 = PASS
Button 8 -> FIRE8 = PASS
Button 1 + Button 8 -> FIRE9 / Big Poof = PASS
Buttons active-HIGH = PASS
FIRE outputs active-LOW = PASS
OLED diagnostics = PASS
Serial event logging = PASS
```

After completion, the temporary schema can be treated as:

```text
bench-tested for India development
```

It is still not final California wiring until the California hardware, relays, OLED, Output Expander wiring, LED zones, and physical sculpture wiring are confirmed.
````
