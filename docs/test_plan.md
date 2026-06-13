# Test Plan

This file defines the staged test plan for the Tardi Controller firmware and hardware integration.

The goal is to prove the controller behavior safely before any live fire hardware is connected.

For the current baseline and detailed GPIO mapping, see:

```text
docs/current_baseline.md
docs/gpio_schema.md
docs/pin_mapping.md
docs/esp32_led_port_status.md
docs/led_output_expander.md
docs/led_animation_architecture.md
```

## 1. Test Goals

The tests should confirm:

```text
ESP32 starts correctly
8 active-HIGH button inputs are read correctly
inputs debounce correctly
new button presses are detected
FIRE1-FIRE8 trigger 500 ms one-shot pulses
holding a button does not keep an output firing
releasing and pressing again triggers another 500 ms pulse
Button 1 + Button 8 triggers FIRE9 / big poof
simulation mode keeps physical FIRE pins inactive
live mode activates physical FIRE pins only during valid pulses
FIRE outputs use active-LOW relay logic
FIRE outputs initialize HIGH
FIRE outputs pulse LOW when triggered
OLED shows useful diagnostic status
Serial debug shows detailed diagnostic status
ESP32 can send LED data to the Pixelblaze Output Expander over UART
LED zone behavior matches the intended interaction state
```

The test plan should not assume that a relay, solenoid, flame, or LED strip physically worked unless that hardware is actually connected and observed.

## 2. Current Bench-Test GPIO Baseline

Detailed GPIO mapping is tracked in:

```text
docs/gpio_schema.md
```

Current temporary button allocation:

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

Current temporary FIRE allocation:

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

Current OLED allocation:

```text
OLED SDA = GPIO1
OLED SCL = GPIO2
```

Latest completed bench tests:

```text
OLED on GPIO1/GPIO2 = PASS
Button input pins = PASS
Button 1 -> FIRE1 = PASS
Button 2 -> FIRE2 = PASS
Button 3 -> FIRE3 = PASS
Button 4 -> FIRE4 = PASS
Button 5 -> FIRE5 = PASS
Button 6 -> FIRE6 = PASS
Button 7 -> FIRE7 = PASS
Button 8 -> FIRE8 = PASS
Button 1 + Button 8 -> FIRE9 / Big Poof = PASS
Big Poof 10-second cutoff = PASS
FIRE1-FIRE9 active-LOW LED output test = PASS
Buttons active-HIGH = PASS
FIRE outputs active-LOW = PASS
OLED diagnostics = PASS
Serial event logging = PASS
Output Expander software simulator = PASS
```

These are current bench-test assignments only.

Final California wiring may still change.

## 3. Test Stages

Testing should happen in this order:

```text
Stage 1: Compile test
Stage 2: Serial-only firmware test
Stage 3: OLED display test
Stage 4: Breadboard input test
Stage 5: Simulation-mode output logic test
Stage 6: One-shot pulse test
Stage 7: Big poof logic test
Stage 8: Live output pin test with safe indicators
Stage 9: LED Output Expander bench test
Stage 10: LED animation / zone test
Stage 11: California direct button wiring integration test
Stage 12: California output relay integration test
Stage 13: Safety stop / cutoff test
Stage 14: Final live-fire commissioning
```

Do not skip ahead to live fire testing.

Do not connect solenoids, poofers, gas, flame hardware, or full LED loads until the relevant earlier electrical tests pass.

## 4. Stage 1: Compile Test

Purpose:

```text
Confirm the firmware compiles for the ESP32-S3 board.
```

Check:

```text
correct board selected
required libraries installed if OLED is enabled
no compile errors
no missing function errors
no pin array size mismatch
button count matches 8
FIRE output count matches 9
OLED pins are centralized
FIRE pins are centralized
LED output / UART configuration is centralized
```

Expected result:

```text
Firmware compiles successfully.
```

If OLED hardware is not connected yet, keep OLED hardware disabled in firmware.

If LED Output Expander hardware is not connected yet, keep LED hardware output disabled or simulated.

## 5. Stage 2: Serial-Only Firmware Test

Purpose:

```text
Confirm the ESP32 boots and reports its mode over Serial.
```

Setup:

```text
ESP32 connected by USB
Serial Monitor open
baud rate 115200
no external fire hardware connected
no relay hardware connected
no LED strip hardware connected
simulation mode enabled
```

Expected startup output should confirm:

```text
controller started
mode is SIMULATION
OLED enabled or disabled state
input pull-down mode
FIRE output idle level is HIGH
FIRE output trigger level is LOW
LED output mode is disabled, simulated, or expander test mode
```

Expected result:

```text
ESP32 repeatedly prints button and fire state diagnostics.
No FIRE output pins should trigger in simulation mode.
No LED hardware output is required at this stage.
```

## 6. Stage 3: OLED Display Test

Purpose:

```text
Confirm the OLED works and displays the agreed diagnostic states.
```

Setup:

```text
OLED connected to ESP32 I2C pins
OLED hardware enabled in firmware
SSD1306 library installed
Adafruit GFX library installed
simulation mode enabled
```

Expected idle simulator screen:

```text
SIMULATOR MODE

READY
Input: -
Output: OFF
LED: -
No live output
```

Expected simulator firing screen for Input 4:

```text
SIMULATOR MODE

FIRING
Input: 4
Output: 4
LED: 4
No live output
```

Expected pulse-complete screen if Input 4 is still held after the 500 ms pulse:

```text
SIMULATOR MODE

PULSE COMPLETE
Input: 4
Output: OFF
LED: 4
No live output
```

Expected big-poof screen:

```text
SIMULATOR MODE

BIG POOF
Input: 1+8
Output: 1 8 9
LED: BIG
No live output
```

The OLED should not permanently show:

```text
raw input masks
GPIO pin numbers
pull-down mode
Pixelblaze internals
Output Expander internals
Plan A / Plan B labels
timer countdowns
decorative graphics
```

Pass condition:

```text
OLED shows readable state, input, output, LED, and mode information.
OLED and Serial agree.
```

## 7. Stage 4: Breadboard Input Test

Purpose:

```text
Confirm each input is read correctly by the ESP32.
```

Setup for India bench testing:

```text
simulation mode enabled
internal pull-down mode enabled
simple button or jumper from ESP32 3.3V to each input pin
no relay hardware required
no live fire hardware connected
```

For each input, test:

```text
Input 1
Input 2
Input 3
Input 4
Input 5
Input 6
Input 7
Input 8
```

Expected result for each input:

```text
Serial shows the correct input active.
OLED shows the correct Input number.
No other input should appear active.
Input returns inactive when released.
```

Failure signs:

```text
input appears active when not pressed
wrong input number appears
multiple inputs appear when only one is pressed
input flickers badly
input never appears
input does not return LOW when released
```

If an input appears active when not pressed, check pull-down configuration and wiring first.

Pass condition:

```text
All 8 inputs read LOW when released.
All 8 inputs read HIGH when pressed or connected to 3.3V.
Each input maps to the correct logical button number.
```

## 8. Stage 5: Simulation-Mode Output Logic Test

Purpose:

```text
Confirm firmware output logic without activating physical FIRE pins.
```

Setup:

```text
simulation mode enabled
no relay hardware connected
no solenoids connected
no live fire hardware connected
```

For each normal input:

```text
Press Input 1 -> FIRE1
Press Input 2 -> FIRE2
Press Input 3 -> FIRE3
Press Input 4 -> FIRE4
Press Input 5 -> FIRE5
Press Input 6 -> FIRE6
Press Input 7 -> FIRE7
Press Input 8 -> FIRE8
```

Expected simulator behavior:

```text
OLED shows FIRING
OLED shows matching Input number
OLED shows matching Output number
OLED shows matching LED number
OLED shows No live output
Serial shows the matching FIRE pulse state
physical FIRE pins do not activate
```

Pass condition:

```text
Each input causes the correct simulated output selection.
No physical FIRE pin activates in simulation mode.
```

## 9. Stage 6: One-Shot Pulse Test

Purpose:

```text
Confirm press-and-hold does not create continuous output.
```

Test:

```text
Press and hold Input 4.
```

Expected behavior:

```text
FIRE4 becomes active in logic for 500 ms.
FIRE4 returns inactive automatically.
Input 4 may remain active if still held.
OLED changes from FIRING to PULSE COMPLETE.
Serial confirms the pulse ended.
```

Then test:

```text
Release Input 4.
Press Input 4 again.
```

Expected behavior:

```text
FIRE4 fires again for another 500 ms.
```

Pass condition:

```text
Each new press creates one 500 ms pulse.
Holding the input does not keep the output active.
```

## 10. Stage 7: Big Poof Logic Test

Purpose:

```text
Confirm Button 1 + Button 8 triggers FIRE9.
```

Test:

```text
Press Input 1 and Input 8 together.
```

Expected current behavior:

```text
Input: 1+8
Output: 1 8 9
LED: BIG
BIG POOF state appears
```

Important current behavior:

```text
Button 1 + Button 8 currently can trigger FIRE1, FIRE8, and FIRE9.
```

This is intentional for the current scaffold.

A later revision may change big-poof behavior so the combo triggers FIRE9 only.

Pass condition:

```text
FIRE9 is triggered by the Button 1 + Button 8 combination.
FIRE9 uses its own one-shot pulse timing.
Holding the combo does not keep FIRE9 active.
FIRE9 does not trigger from Button 1 alone.
FIRE9 does not trigger from Button 8 alone.
```

## 11. Stage 8: Live Output Pin Test With Safe Indicators

Purpose:

```text
Confirm live mode drives ESP32 FIRE output pins correctly.
```

Setup:

```text
no solenoids connected
no live fire hardware connected
use LEDs, meter, oscilloscope, or safe relay test board only
live mode enabled deliberately
```

For active-LOW LED testing, safe bench wiring is:

```text
3V3 -> resistor -> LED long leg
LED short leg -> ESP32 FIRE GPIO
```

Expected live mode screen:

```text
LIVE MODE
```

For Input 4 during active pulse:

```text
LIVE MODE

FIRING
Input: 4
Output: 4
LED: 4
Live output: ON
```

Expected behavior:

```text
FIRE4 pin starts HIGH in idle state.
FIRE4 pin goes LOW during the 500 ms pulse.
FIRE4 pin returns HIGH after the pulse.
Holding Input 4 does not keep FIRE4 LOW.
```

Repeat for:

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

Pass condition:

```text
Every FIRE output pin starts HIGH in idle state.
Every FIRE output pin goes LOW only for its intended pulse.
Every FIRE output pin returns HIGH automatically.
No unrequested FIRE output triggers.
```

## 12. Stage 9: LED Output Expander Bench Test

Purpose:

```text
Confirm ESP32-S3 can send LED data to the standard Pixelblaze Output Expander v3.0 over UART.
```

Current intended LED signal path:

```text
ESP32-S3 -> UART -> Pixelblaze Output Expander v3.0 -> LED zones
```

### India-side simulator test

This test does not require the physical Output Expander.

Setup:

```text
compile firmware in Arduino IDE
upload to ESP32-S3
open Serial Monitor at 115200
leave real Output Expander output disabled
no LED strip hardware required
```

Expected Serial diagnostics:

```text
EXP REAL allowed=0 started=0 tx=39 baud=2000000
EXP SIM OK channels=8 pixels=2008 failed=0 ... byteOrder=GRB
```

Pass condition:

```text
real output allowed remains 0
real output started remains 0
simulator reports 8 channels
simulator reports 2008 pixels
simulator reports 0 failed pixels
byte order reports GRB
```

The checksum varies with animation time/state and is not a fixed universal value.

### California-only real hardware validation

Setup:

```text
no fire relay hardware connected
no solenoids connected
no live fire hardware connected
LED power system checked separately
ESP32 and LED data system share the required signal reference
Output Expander connected according to its wiring plan
LED output test mode enabled deliberately
```

Before this stage, confirm:

```text
ESP32 GPIO39 TX connects to Output Expander data input
which ground/reference connection is required
how the Output Expander is powered
how the LED strips are powered
which LED output channel is zone 1
which LED output channel is zone 8
```

Do not use board-labelled TX/RX / UART0.
Do not use GPIO16 because it is Button 6.

Expected result:

```text
ESP32 sends known LED test data.
Output Expander produces LED data on the intended channel.
Only the intended LED zone responds.
No FIRE output behavior changes during LED testing.
```

Suggested first LED tests:

```text
Zone 1 only
Zone 2 only
Zone 3 only
Zone 4 only
Zone 5 only
Zone 6 only
Zone 7 only
Zone 8 only
all zones low brightness
all zones off
```

Pass condition:

```text
ESP32 UART output reaches the Output Expander.
The Output Expander drives the intended LED zone.
Zone numbering is understood and recorded.
No wrong LED zone activates.
No fire output is affected.
```

## 13. Stage 10: LED Animation / Zone Test

Purpose:

```text
Confirm LED animation behavior responds to controller interaction state.
```

The LED system should follow the same logical interaction state used by FIRE1-FIRE9.

Expected logical behavior:

```text
Button 1 press -> LED zone 1 animation
Button 2 press -> LED zone 2 animation
Button 3 press -> LED zone 3 animation
Button 4 press -> LED zone 4 animation
Button 5 press -> LED zone 5 animation
Button 6 press -> LED zone 6 animation
Button 7 press -> LED zone 7 animation
Button 8 press -> LED zone 8 animation
Button 1 + Button 8 -> BIG LED animation
```

Expected OLED wording should stay architecture-neutral:

```text
LED: 1
LED: 2
LED: 3
LED: 4
LED: 5
LED: 6
LED: 7
LED: 8
LED: BIG
```

Do not change the OLED to say:

```text
Pixelblaze: 4
Output Expander: 4
```

because that would make the display language hardware-specific.

Pass condition:

```text
The same input/output event produces the expected LED zone behavior.
The OLED remains readable and architecture-neutral.
Serial debug confirms the LED zone or animation request.
```

## 14. Stage 11: California Direct Button Wiring Integration Test

Purpose:

```text
Confirm the real button panel produces clean ESP32 3.3V input signals.
```

Preferred setup:

```text
ESP32 3.3V sent to the button panel
one shared 3.3V wire to the button panel
one return wire from each button back to its ESP32 GPIO input
one shared GND
external 10kΩ pull-downs installed on the ESP32 input lines
firmware set to external pull-down mode
simulation mode enabled
no live fire output hardware connected
```

For each physical button:

```text
press the physical button
observe OLED Input number
observe Serial input state
release the button
confirm input returns inactive
```

Expected result:

```text
Each physical button produces the correct ESP32 input number.
Released inputs return LOW.
No input floats when released.
No input receives 5V.
ESP32 and button signal circuit share DC ground.
```

Pass condition:

```text
All 8 physical buttons read correctly and return LOW when released.
```

## 15. Stage 12: California Output Relay Integration Test

Purpose:

```text
Confirm ESP32 output pins can drive the output relay interface correctly.
```

Setup:

```text
live fire hardware still disabled
output relay board connected only for safe electrical testing
live mode enabled only when ready
```

For each FIRE output:

```text
trigger the matching input
intended output relay channel activates briefly
confirm the relay channel turns off after 500 ms
confirm holding input does not keep relay active
```

Expected relay behavior:

```text
FIRE output idle state = HIGH
FIRE output trigger state = LOW
relay activates when ESP32 output goes LOW
relay deactivates when ESP32 output returns HIGH
```

Expected result:

```text
Input 1 -> FIRE1 relay pulse
Input 2 -> FIRE2 relay pulse
Input 3 -> FIRE3 relay pulse
Input 4 -> FIRE4 relay pulse
Input 5 -> FIRE5 relay pulse
Input 6 -> FIRE6 relay pulse
Input 7 -> FIRE7 relay pulse
Input 8 -> FIRE8 relay pulse
Input 1+8 -> FIRE9 relay pulse
```

Pass condition:

```text
Each output relay follows the ESP32 active-LOW output pulse correctly.
No output relay sticks on.
No wrong output relay activates.
```

## 16. Stage 13: Safety Stop / Cutoff Test

Purpose:

```text
Confirm abnormal output states are forced back to idle.
```

The normal 500 ms pulse should end long before the 10 second backup cutoff.

The backup cutoff is only a guard against unexpected behavior.

If a cutoff or unexpected output state is detected, OLED should show:

```text
SAFETY STOP

Output 4
forced OFF
```

Pass condition:

```text
Any output that remains active unexpectedly is returned HIGH.
Serial reports the safety event.
OLED shows SAFETY STOP.
```

## 17. Stage 14: Final Live-Fire Commissioning

Purpose:

```text
Confirm the full sculpture behavior under controlled live-fire conditions.
```

This stage is for the California team only.

Before this stage:

```text
all input tests must pass
all output relay tests must pass
simulation mode behavior must be confirmed
live output pin behavior must be confirmed without solenoids
FIRE outputs must be confirmed HIGH when idle
FIRE outputs must be confirmed LOW only during valid pulses
output relays must be confirmed to turn off correctly
LED Output Expander behavior must be understood if LEDs are active during commissioning
LED zones must be confirmed if LEDs are active during commissioning
emergency stop / physical safety systems must be confirmed
gas/flame safety review must be complete
```

Live-fire test order:

```text
test one poofer at a time
verify 500 ms pulse
verify no hold-to-fire behavior
verify output returns idle
verify big poof only after normal outputs are understood
verify LED behavior separately from flame behavior if needed
```

Pass condition:

```text
Each live-fire output behaves exactly like the tested electrical output:
one valid trigger creates one controlled active-LOW relay pulse.
```

## 18. General Pass / Fail Rules

A test passes only if:

```text
the expected input number appears
the expected output number appears
the expected LED value appears
the output pulse ends automatically
FIRE outputs return HIGH after each pulse
no unintended output activates
simulation mode keeps live outputs disabled
live mode activates only the intended output
Serial and OLED agree
LED tests do not affect FIRE outputs
FIRE tests do not depend on LED output hardware
```

A test fails if:

```text
wrong input appears
wrong output appears
wrong LED zone appears
output remains active while held
output does not return HIGH after pulse
multiple unexpected outputs appear
input floats when released
OLED and Serial disagree
simulation mode activates a physical output
live output appears without a valid input
LED testing changes FIRE behavior
FIRE testing requires LED hardware to work
```

## 19. Notes To Record During Testing

Record:

```text
date
firmware version or commit
ESP32 board used
OLED connected or disabled
simulation/live mode
internal/external pull-down mode
which input was tested
which output appeared
which LED zone appeared
whether pulse ended correctly
whether FIRE output returned HIGH after pulse
any unexpected behavior
```

For California button integration, also record:

```text
button wiring used
button cable type
whether shared 3.3V is confirmed
whether shared GND is confirmed
whether each button return line is confirmed
external pull-down arrangement used
any wiring changes
```

For California relay integration, also record:

```text
output relay board used
relay trigger voltage
whether ESP32 output reliably controls the relay input
whether relay idle is confirmed HIGH
whether relay trigger is confirmed LOW
any wrong or stuck relay behavior
```

For LED Output Expander integration, also record:

```text
ESP32 UART pins used
Output Expander input wiring
Output Expander power wiring
LED power supply used
LED zone count
LED count per zone
LED zone-to-output-channel mapping
whether each zone responds correctly
any wrong-zone behavior
```

## 20. Do Not Test Yet

Do not test these until the earlier stages pass:

```text
live fire hardware
gas solenoids
full sculpture live mode
full-brightness LED loads
long-running LED animations
Pixelblaze replacement assumptions beyond the Output Expander role
```

The current priority is:

```text
safe input reading
correct one-shot active-LOW output logic
clear OLED diagnostics
reliable simulation/live separation
ESP32 UART output to the Pixelblaze Output Expander
controlled LED zone testing
```
````
